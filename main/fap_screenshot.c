#include "fap_screenshot.h"

#include "bsp_display.h"
#include "bsp_pins.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fap_screenshot_protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>

#define SCREENSHOT_WIDTH (BSP_LCD_W / 2)
#define SCREENSHOT_HEIGHT (BSP_LCD_H / 2)
#define SCREENSHOT_PIXELS ((size_t)SCREENSHOT_WIDTH * SCREENSHOT_HEIGHT)
#define SCREENSHOT_BYTES (SCREENSHOT_PIXELS * sizeof(uint16_t))
#define SCREENSHOT_CHUNK 512

static const char *TAG = "fap_screen";
/* Capture only when requested. The former always-on RGB565 copy and
 * coverage map reserved 40,800 bytes and processed every display flush. */
static uint16_t s_capture_row[SCREENSHOT_WIDTH];
static int64_t s_capture_deadline;
static bool s_capturing;
static bool s_capture_ok;
static unsigned s_next_row;

static bool write_all(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    size_t sent = 0;
    while (sent < size) {
        size_t remaining = size - sent;
        size_t chunk = remaining < SCREENSHOT_CHUNK ? remaining : SCREENSHOT_CHUNK;
        if (esp_timer_get_time() >= s_capture_deadline) return false;
        int written = usb_serial_jtag_write_bytes(bytes + sent, chunk, pdMS_TO_TICKS(50));
        if (written != (int)chunk) return false;
        sent += chunk;
    }
    return true;
}

static void finish_capture(bool success)
{
    s_capture_ok = success;
    s_capturing = false;
}

static void observe_flush(lv_event_t *event)
{
    if (!s_capturing) return;
    lv_display_t *display = lv_event_get_target(event);
    const lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *source = lv_display_get_buf_active(display);
    if (!area || !source || source->header.cf != LV_COLOR_FORMAT_RGB565 ||
        area->x1 != 0 || area->x2 != BSP_LCD_W - 1 ||
        source->header.w < BSP_LCD_W || source->header.h < (unsigned)(area->y2 - area->y1 + 1)) {
        finish_capture(false);
        return;
    }
    /* A full-screen invalidation supplies top-to-bottom bands. Read before
     * esp_lvgl_port swaps bytes or hands this buffer to DMA. No persistent
     * framebuffer is needed; the requested capture still pauses UI drawing. */
    for (int y = area->y1; y <= area->y2; ++y) {
        if (y & 1) continue;
        if (y < (int)s_next_row * 2) continue;
        if (y != (int)s_next_row * 2 || s_next_row >= SCREENSHOT_HEIGHT) {
            finish_capture(false);
            return;
        }
        const uint16_t *row = (const uint16_t *)(source->data +
                              (size_t)(y - area->y1) * source->header.stride);
        for (unsigned x = 0; x < SCREENSHOT_WIDTH; ++x) s_capture_row[x] = row[x * 2];
        if (!write_all(s_capture_row, sizeof(s_capture_row))) {
            finish_capture(false);
            return;
        }
        ++s_next_row;
    }
    if (s_next_row == SCREENSHOT_HEIGHT) finish_capture(true);
}

static void send_screen(void)
{
    if (!bsp_lvgl_lock(1000)) return;
    lv_obj_t *screen = lv_screen_active();
    if (!screen) { bsp_lvgl_unlock(); return; }
    char header[72];
    int header_size = snprintf(header, sizeof(header), "FAP_SCREENSHOT_V1 %u %u RGB565LE %u\n",
                               (unsigned)SCREENSHOT_WIDTH, (unsigned)SCREENSHOT_HEIGHT,
                               (unsigned)SCREENSHOT_BYTES);
    if (header_size <= 0 || (size_t)header_size >= sizeof(header)) {
        bsp_lvgl_unlock();
        return;
    }
    esp_log_level_t previous_level = esp_log_get_default_level();
    esp_log_level_set("*", ESP_LOG_NONE);
    s_capture_deadline = esp_timer_get_time() + 2000000;
    s_next_row = 0;
    s_capture_ok = false;
    s_capturing = write_all(header, (size_t)header_size);
    /* Serialize the one requested full refresh under the LVGL lock. The
     * capture worker owns the request lifetime; no late callback can write
     * binary pixels after timeout/log restoration. USB writes are bounded by
     * a two-second request deadline and fail on 50 ms of backpressure. */
    if (s_capturing) {
        lv_obj_invalidate(screen);
        lv_refr_now(lv_display_get_default());
    }
    bool sent = s_capture_ok;
    s_capturing = false;
    bsp_lvgl_unlock();
    if (sent) sent = usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(1000)) == ESP_OK;
    esp_log_level_set("*", previous_level);
    if (!sent) ESP_LOGW(TAG, "Screen capture transfer failed");
}

static void screenshot_task(void *argument)
{
    (void)argument;
    fap_screenshot_matcher_t matcher = {0};
    uint8_t input[32];
    while (true) {
        if (!usb_serial_jtag_is_driver_installed()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        int received = usb_serial_jtag_read_bytes(input, sizeof(input), pdMS_TO_TICKS(100));
        if (received < 0) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        for (int index = 0; index < received; ++index) {
            if (fap_screenshot_matcher_push(&matcher, input[index])) send_screen();
        }
        if (received == 0) vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t fap_screenshot_start(void)
{
    if (!bsp_lvgl_lock(1000)) return ESP_ERR_TIMEOUT;
    lv_display_t *display = lv_display_get_default();
    lv_obj_t *screen = lv_screen_active();
    if (!display || !screen) {
        bsp_lvgl_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    lv_display_add_event_cb(display, observe_flush, LV_EVENT_FLUSH_START, NULL);
    lv_obj_invalidate(screen);
    bsp_lvgl_unlock();

    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t configuration = {
            .tx_buffer_size = 1024,
            .rx_buffer_size = 256,
        };
        esp_err_t result = usb_serial_jtag_driver_install(&configuration);
        if (result != ESP_OK) return result;
    }
    usb_serial_jtag_vfs_use_driver();
    return xTaskCreate(screenshot_task, "fap_screen", 8192, NULL, 3, NULL) == pdPASS
        ? ESP_OK : ESP_ERR_NO_MEM;
}
