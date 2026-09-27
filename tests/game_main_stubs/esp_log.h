#pragma once

static inline void esp_log_stub(const char *tag, const char *fmt, ...) {
    (void)tag;
    (void)fmt;
}

#define ESP_LOGE(...) esp_log_stub(__VA_ARGS__)
#define ESP_LOGW(...) esp_log_stub(__VA_ARGS__)
#define ESP_LOGI(...) esp_log_stub(__VA_ARGS__)
#define ESP_LOGD(...) esp_log_stub(__VA_ARGS__)
