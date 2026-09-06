#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t matched;
} fap_screenshot_matcher_t;

void fap_screenshot_matcher_reset(fap_screenshot_matcher_t *matcher);
bool fap_screenshot_matcher_push(fap_screenshot_matcher_t *matcher, uint8_t byte);
