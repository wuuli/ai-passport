#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t matched;
} fap_screenshot_matcher_t;

void fap_screenshot_matcher_reset(fap_screenshot_matcher_t *matcher);
bool fap_screenshot_matcher_push(fap_screenshot_matcher_t *matcher, uint8_t byte);

/* Exact, newline-terminated USB commands; no arbitrary memory or code access. */
typedef enum { FAP_INPUT_NONE=-1, FAP_INPUT_LEFT, FAP_INPUT_RIGHT, FAP_INPUT_OK, FAP_INPUT_BACK } fap_input_t;
fap_input_t fap_input_parse(const char *line);
