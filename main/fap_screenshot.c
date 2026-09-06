#include "fap_screenshot.h"

#include "bsp_display.h"
#include "bsp_pins.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
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
static uint16_t s_pixels[SCREENSHOT_PIXELS] __attribute__((aligned(64)));
static uint8_t s_coverage[(SCREENSHOT_PIXELS + 7) / 8];
static size_t s_covered_pixels;

static bool write_all(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    size_t sent = 0;
    while (sent < size) {
        size_t remaining = size - sent;
        size_t chunk = remaining < SCREENSHOT_CHUNK ? remaining : SCREENSHOT_CHUNK;
        int written = usb_serial_jtag_write_bytes(bytes + sent, chunk, pdMS_TO_TICKS(5000));
        if (written != (int)chunk) return false;
        sent += chunk;
    }
    return true;
}

static void observe_flush(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);
    const lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *source = lv_display_get_buf_active(display);
    if (!area || !source || source->header.cf != LV_COLOR_FORMAT_RGB565) return;

    for (unsigned destination_y = 0; destination_y < SCREENSHOT_HEIGHT; ++destination_y) {
        int source_y = (int)destination_y * 2;
        if (source_y < area->y1 || source_y > area->y2) continue;
        for (unsigned destination_x = 0; destination_x < SCREENSHOT_WIDTH; ++destination_x) {
            int source_x = (int)destination_x * 2;
            if (source_x < area->x1 || source_x > area->x2) continue;
            unsigned local_x = (unsigned)(source_x - area->x1);
            unsigned local_y = (unsigned)(source_y - area->y1);
            if (local_x >= source->header.w || local_y >= source->header.h) continue;
            const uint8_t *row = source->data + (size_t)local_y * source->header.stride;
            size_t destination = (size_t)destination_y * SCREENSHOT_WIDTH + destination_x;
            s_pixels[destination] = ((const uint16_t *)row)[local_x];
            uint8_t mask = (uint8_t)(1U << (destination % 8));
            if (!(s_coverage[destination / 8] & mask)) {
                s_coverage[destination / 8] |= mask;
                s_covered_pixels++;
            }
        }
    }
}

static void send_screen(void)
{
    if (!bsp_lvgl_lock(1000)) return;
    if (s_covered_pixels != SCREENSHOT_PIXELS) {
        bsp_lvgl_unlock();
        ESP_LOGW(TAG, "Screen capture is not ready");
        return;
    }

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
    bool sent = write_all(header, (size_t)header_size) &&
                write_all(s_pixels, SCREENSHOT_BYTES) &&
                usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(10000)) == ESP_OK;
    esp_log_level_set("*", previous_level);
    bsp_lvgl_unlock();
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
