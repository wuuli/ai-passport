#pragma once

#include "esp_err.h"
#include "fap_screenshot_protocol.h"

typedef void (*fap_input_cb_t)(fap_input_t input);
esp_err_t fap_screenshot_start(fap_input_cb_t input_cb);
