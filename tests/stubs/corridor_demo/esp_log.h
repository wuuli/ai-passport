#pragma once
#ifndef asm
#define asm __asm__
#endif
static inline void esp_log_stub(const char *tag, const char *fmt, ...) {
    (void)tag; (void)fmt;
}
#define ESP_LOGI(...) esp_log_stub(__VA_ARGS__)
#define ESP_LOGW(...) esp_log_stub(__VA_ARGS__)
#define ESP_LOGE(...) esp_log_stub(__VA_ARGS__)
