// components/bsp/src/bsp_display_lvgl.c
// LVGL 接入单独成文件:不用 LVGL 的开发者删掉本文件 + idf_component.yml 里的两条依赖即可。
#include "bsp_display.h"
#include "bsp_display_rounding.h"
#include "bsp_pins.h"
#include "esp_lcd_panel_io.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "bsp_lvgl";

#define BSP_LVGL_DRAW_BUFFER_LINES 40

static lv_display_t *s_disp;
static SemaphoreHandle_t s_flush_sem;
static bool s_port_initialized;
static bool s_port_init_failed;
static bool s_io_cb_live;

static bool bsp_lvgl_color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                         esp_lcd_panel_io_event_data_t *edata,
                                         void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    BaseType_t high_task_woken = pdFALSE;
    SemaphoreHandle_t sem = (SemaphoreHandle_t)user_ctx;
    if (sem) {
        // Signal completion via semaphore only; do not call lv_display_flush_ready()
        // here so LVGL's wait_for_flushing() invokes flush_wait_cb and clears
        // disp->flushing after blocking/taking the token.
        xSemaphoreGiveFromISR(sem, &high_task_woken);
    }
    return high_task_woken == pdTRUE;
}

static void bsp_lvgl_flush_wait_cb(lv_display_t *disp)
{
    (void)disp;
    if (s_flush_sem) {
        (void)xSemaphoreTake(s_flush_sem, portMAX_DELAY);
    }
}

static void rounded_flush_event(lv_event_t *event)
{
    lv_display_t *disp = lv_event_get_target(event);
    const lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *draw_buf = lv_display_get_buf_active(disp);
    if (!area || !draw_buf || !draw_buf->data ||
        lv_display_get_color_format(disp) != LV_COLOR_FORMAT_RGB565) {
        return;
    }

    const int32_t width = lv_area_get_width(area);
    if (draw_buf->header.stride < (uint32_t)width * sizeof(uint16_t)) return;

    for (int32_t y = area->y1; y <= area->y2; ++y) {
        uint16_t *row = (uint16_t *)(draw_buf->data +
                                     (y - area->y1) * draw_buf->header.stride);
        int32_t visible_x1;
        int32_t visible_x2;
        if (!bsp_display_rounded_row_span(y, BSP_LCD_W, BSP_LCD_H,
                                          BSP_LVGL_SCREEN_RADIUS, &visible_x1,
                                          &visible_x2)) {
            memset(row, 0, (size_t)width * sizeof(uint16_t));
            continue;
        }
        // Only clear pixels outside the visible span. The port swaps RGB565
        // bytes after this event; black is 0 in either byte order.
        const int32_t clear_left_end = visible_x1 > area->x2 ? area->x2 : visible_x1 - 1;
        const int32_t clear_right_start = visible_x2 < area->x1 ? area->x1 : visible_x2 + 1;
        for (int32_t x = area->x1; x <= clear_left_end; ++x) {
            row[x - area->x1] = 0;
        }
        for (int32_t x = clear_right_start; x <= area->x2; ++x) {
            row[x - area->x1] = 0;
        }
    }

}

lv_display_t *bsp_lvgl_init(void) {
    if (s_disp) return s_disp;
    if (s_port_init_failed) {
        ESP_LOGE(TAG, "LVGL 初始化未完成或处于不可恢复故障，需重启后重试");
        return NULL;
    }
    if (!bsp_display_panel() || !bsp_display_io()) {
        ESP_LOGE(TAG, "请先成功调用 bsp_display_init()");
        return NULL;
    }

    if (!s_port_initialized) {
        // Port 2.9.0 has no public completion handshake for asynchronous deinit.
        // Never overwrite a possibly live context after a partial port failure.
        const lvgl_port_cfg_t pc = ESP_LVGL_PORT_INIT_CONFIG();
        if (lvgl_port_init(&pc) != ESP_OK) {
            s_port_init_failed = true;
            ESP_LOGE(TAG, "lvgl_port_init 失败，需重启后重试");
            return NULL;
        }
        s_port_initialized = true;
    }

    if (!s_flush_sem) {
        s_flush_sem = xSemaphoreCreateBinary();
        if (!s_flush_sem) {
            ESP_LOGE(TAG, "创建 LVGL flush 信号量失败");
            return NULL;
        }
    } else {
        (void)xSemaphoreTake(s_flush_sem, 0);
    }

    const lvgl_port_display_cfg_t dc = {
        .panel_handle = bsp_display_panel(),
        .io_handle    = bsp_display_io(),
        // ⚠ C3 无 PSRAM,DMA 只能用内部 RAM。40 行单缓冲约 19.2KB，
        // 可减少窗口命令和队列提交次数；仍保留单缓冲，避免双缓冲挤压音频/Wi-Fi。
        .buffer_size   = (uint32_t)BSP_LCD_W * BSP_LVGL_DRAW_BUFFER_LINES,
        .double_buffer = false,
        .hres = BSP_LCD_W, .vres = BSP_LCD_H,
        // 旋转/镜像必须在这里配:esp_lvgl_port 注册显示时会重新下发 MADCTL,
        // 覆盖 bsp_display.c 里 esp_lcd_panel_mirror() 的设置。
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        // swap_bytes:LVGL 输出小端 RGB565,ST7789 走 SPI 要大端 → 需交换高低字节。
        .flags = { .buff_dma = true, .swap_bytes = true },
    };
    // The port mutex is recursive. Keep registration, flush-wait setup, and the
    // mask callback in one critical section, before the new display can produce
    // its first flush.
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "LVGL 初始化加锁失败");
        if (!s_io_cb_live) {
            vSemaphoreDelete(s_flush_sem);
            s_flush_sem = NULL;
        }
        return NULL;
    }
    lv_display_t *disp = lvgl_port_add_disp(&dc);
    bool io_cb_registered = false;
    bool mask_registered = false;
    if (disp) {
        const esp_lcd_panel_io_callbacks_t cbs = {
            .on_color_trans_done = bsp_lvgl_color_trans_done_cb,
        };
        if (esp_lcd_panel_io_register_event_callbacks(bsp_display_io(), &cbs, s_flush_sem) == ESP_OK) {
            io_cb_registered = true;
            s_io_cb_live = true;
            lv_display_set_flush_wait_cb(disp, bsp_lvgl_flush_wait_cb);
            // LVGL 9.5 returns void here; check the list while still holding the lock.
            const uint32_t count = lv_display_get_event_count(disp);
            lv_display_add_event_cb(disp, rounded_flush_event, LV_EVENT_FLUSH_START, NULL);
            mask_registered = lv_display_get_event_count(disp) == count + 1;
        }
    }
    if (!io_cb_registered || !mask_registered) {
        ESP_LOGE(TAG, "LVGL display、flush 同步或圆角回调注册失败");
        // esp_lvgl_port_add_disp already registered a default callback referencing disp;
        // clear it before removing display. If unregister fails, retain display and
        // semaphore and delete the refresh timer so no ISR accesses a dangling callback
        // or freed context, and mark port failed to disallow unsafe retry.
        const esp_lcd_panel_io_callbacks_t empty_cbs = { 0 };
        esp_err_t unreg_err = esp_lcd_panel_io_register_event_callbacks(bsp_display_io(), &empty_cbs, NULL);
        if (unreg_err == ESP_OK) {
            s_io_cb_live = false;
            if (disp) {
                lv_display_set_flush_wait_cb(disp, NULL);
                lvgl_port_remove_disp(disp);
            }
            if (s_flush_sem) {
                vSemaphoreDelete(s_flush_sem);
                s_flush_sem = NULL;
            }
        } else {
            ESP_LOGE(TAG, "注销 LCD IO 回调失败，保留 display 与信号量以防 ISR 野指针");
            if (disp) {
                lv_display_delete_refr_timer(disp);
            }
            s_port_init_failed = true;
        }
        lvgl_port_unlock();
        // Retain the initialized port for retry when safe.
        return NULL;
    }

    // Mask the final RGB565 flush instead of using root-screen clip_corner.
    // Full-screen rounded clipping creates an ARGB layer that does not fit the
    // 24 KB LVGL pool reliably on this no-PSRAM target.
    s_disp = disp;
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL 就绪，全局圆角=%d，外部填充=黑色", BSP_LVGL_SCREEN_RADIUS);
    return s_disp;
}

bool bsp_lvgl_lock(int timeout_ms) {
    if (!s_disp) return false;
    return lvgl_port_lock(timeout_ms);
}
void bsp_lvgl_unlock(void) {
    if (s_disp) lvgl_port_unlock();
}
