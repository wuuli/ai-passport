// main/main.c —— FoloToy AI Passport BSP 驱动参考示例:初始化 + 菜单 + 按键分发。
//
// 按键语义(全局统一):
//   上/下 短按   菜单中=移动选中项;演示页中=该页自定义
//   确定  短按   菜单中=进入选中项;演示页中=该页自定义
//   确定  长按   演示页中=返回菜单(由本文件统一拦截)
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"      // 错误日志里要打印 BSP_LCD_* 引脚号
#include "demo.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "duel_io.h"
#include "fap_screenshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdatomic.h>

static const char *TAG = "main";

static const demo_entry_t DEMOS[] = {
    {.name = "Display", .enter = demo_display_enter, .exit = demo_display_exit, .key = demo_display_key},
    {.name = "Button", .enter = demo_button_enter, .exit = demo_button_exit, .key = demo_button_key},
    {.name = "Audio", .enter = demo_audio_enter, .exit = demo_audio_exit, .key = demo_audio_key},
    {.name = "Battery", .enter = demo_battery_enter, .exit = demo_battery_exit, .key = demo_battery_key},
    {.name = "Wi-Fi", .enter = demo_wifi_enter, .exit = demo_wifi_exit, .key = demo_wifi_key},
    {.name = "BLE", .enter = demo_ble_enter, .exit = demo_ble_exit, .key = demo_ble_key},
    {.name = "Low Power", .enter = demo_low_power_enter, .exit = demo_low_power_exit, .key = demo_low_power_key},
    {.name = "Challenge", .enter = demo_duel_enter, .exit = demo_duel_exit, .key = demo_duel_key,
     .key_at = demo_duel_key_at, .tick = demo_duel_tick},
    {.name = "Corridor", .enter = demo_corridor_enter, .exit = demo_corridor_exit,
     .key = demo_corridor_key, .tick = demo_corridor_tick},
};
#define DEMO_COUNT (sizeof(DEMOS) / sizeof(DEMOS[0]))

// 各外设初始化结果:失败的项在菜单里标 [FAIL] 且不允许进入。
static bool s_ok[DEMO_COUNT];

static lv_obj_t *s_menu_scr;
static lv_obj_t *s_cards[DEMO_COUNT];
static lv_obj_t *s_rows[DEMO_COUNT];
static lv_obj_t *s_mascot;
static int  s_sel;                 // 当前选中项
static int  s_active = -1;         // 当前所在演示页;-1 = 在菜单

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
    int64_t timestamp_us;
    unsigned generation;
} input_message_t;

static QueueHandle_t s_input_queue;
static atomic_uint s_generation;
static atomic_bool s_input_overflow;

static void menu_refresh(void) {
    for (size_t i = 0; i < DEMO_COUNT; i++) {
        lv_label_set_text_fmt(s_rows[i], "%s%s",
                              DEMOS[i].name,
                              s_ok[i] ? "" : "  [FAIL]");
        ui_pixel_set_selected(s_cards[i], (int)i == s_sel, s_ok[i]);
        lv_obj_set_style_text_color(s_rows[i],
            s_ok[i] ? lv_color_hex(UI_INK) : lv_color_hex(0x7A2020), 0);
    }
}

static void menu_build(void) {
    s_menu_scr = ui_pixel_screen_create("FoloToy");

    for (size_t i = 0; i < DEMO_COUNT; i++) {
        int x = 11 + (int)(i % 2) * 112;
        int y = 52 + (int)(i / 2) * 37;
        s_cards[i] = ui_pixel_panel_create(s_menu_scr, x, y, 102, 32, UI_PAPER);
        s_rows[i] = lv_label_create(s_cards[i]);
        lv_obj_set_style_text_font(s_rows[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(s_rows[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_rows[i]);
    }

    s_mascot = ui_pixel_mascot_create(s_menu_scr, 101, 242);

    menu_refresh();
    lv_screen_load(s_menu_scr);
}

static void enter_menu(void) {
    s_active = -1;
    menu_build();
}

static void process_key(bsp_btn_t btn, bsp_btn_ev_t ev, int64_t timestamp_us) {
    if (s_active >= 0) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {     // 统一返回
            atomic_fetch_add(&s_generation, 1);
            DEMOS[s_active].exit();
            enter_menu();
        } else if (DEMOS[s_active].key_at) {
            DEMOS[s_active].key_at(btn, ev, timestamp_us);
        } else {
            DEMOS[s_active].key(btn, ev);
        }
    } else if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP)   { s_sel = (s_sel + DEMO_COUNT - 1) % DEMO_COUNT; menu_refresh(); }
        if (btn == BSP_BTN_DOWN) { s_sel = (s_sel + 1) % DEMO_COUNT;              menu_refresh(); }
        if (btn == BSP_BTN_OK && s_ok[s_sel]) {
            atomic_fetch_add(&s_generation, 1);
            s_active = s_sel;
            ui_pixel_mascot_jump(s_mascot);
            lv_obj_delete(s_menu_scr);
            s_menu_scr = NULL;
            s_mascot = NULL;
            DEMOS[s_active].enter();
        } else if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            ui_pixel_mascot_jump(s_mascot);
        }
    }
}

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user) {
    (void)user;
    const input_message_t message = {
        .button = button,
        .event = event,
        .timestamp_us = esp_timer_get_time(),
        .generation = atomic_load(&s_generation),
    };
    if (s_input_queue && xQueueSend(s_input_queue, &message, 0) != pdTRUE) {
        atomic_store(&s_input_overflow, true);
    }
}

static void process_input(lv_timer_t *timer) {
    (void)timer;
    const int64_t batch_time_us = esp_timer_get_time();
    if (atomic_exchange(&s_input_overflow, false)) {
        xQueueReset(s_input_queue);
        if (s_active >= 0 && DEMOS[s_active].key_at == demo_duel_key_at) {
            demo_duel_input_lost(batch_time_us);
        }
        if (s_active >= 0 && DEMOS[s_active].tick == demo_corridor_tick) demo_corridor_input_lost();
        ESP_LOGW(TAG, "Input queue overflow; active game stopped");
    }
    input_message_t message;
    for (unsigned count = 0; count < 32 && xQueueReceive(s_input_queue, &message, 0) == pdTRUE; ++count) {
        if (message.generation == atomic_load(&s_generation)) {
            process_key(message.button, message.event, message.timestamp_us);
        }
    }
    if (s_active >= 0 && DEMOS[s_active].tick) DEMOS[s_active].tick(batch_time_us);
}

void app_main(void) {
    ESP_LOGI(TAG, "FoloToy AI Passport BSP demo 启动");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "休眠唤醒原因: %d", wakeup);
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    // 屏幕是本 demo 的 UI 载体,失败就没有菜单可言 —— 打清楚日志后退出,
    // 不做"串口菜单"降级(那会让本文件复杂一倍,违背参考示例的初衷)。
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败,demo 无法继续。"
                      "检查 SPI 接线(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    // 其余外设单项失败不阻塞:菜单里标 [FAIL],其他项照常可测。
    s_ok[0] = true;                                   // Display 已确认可用
    s_input_queue = xQueueCreate(32, sizeof(input_message_t));
    s_ok[1] = s_input_queue && (bsp_button_init(on_key, NULL) == ESP_OK);
    s_ok[2] = (bsp_audio_init() == ESP_OK);
    s_ok[3] = (bsp_battery_init() == ESP_OK);
    s_ok[4] = true;                                    // 页面内按需初始化并显示错误
    s_ok[5] = true;
    s_ok[6] = true;
    s_ok[7] = s_ok[1];
    s_ok[8] = s_ok[1];
    if (!duel_io_init(s_ok[2], s_ok[3])) ESP_LOGW(TAG, "Challenge sound/battery worker unavailable");

    if (bsp_lvgl_lock(1000)) {
        s_sel = 8;
        enter_menu();
        if (s_input_queue) {
            xQueueReset(s_input_queue);
            lv_timer_create(process_input, 10, NULL);
        }
        bsp_lvgl_unlock();
    }
    if (fap_screenshot_start() != ESP_OK) ESP_LOGW(TAG, "Community screenshot service unavailable");

    ESP_LOGI(TAG, "就绪:Display=%d Button=%d Audio=%d Battery=%d",
             s_ok[0], s_ok[1], s_ok[2], s_ok[3]);
}
