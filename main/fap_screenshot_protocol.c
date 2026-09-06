#include "fap_screenshot_protocol.h"

#include <string.h>

static const char SCREENSHOT_COMMAND[] = "FAP_SCREENSHOT_V1";

void fap_screenshot_matcher_reset(fap_screenshot_matcher_t *matcher)
{
    matcher->matched = 0;
}

bool fap_screenshot_matcher_push(fap_screenshot_matcher_t *matcher, uint8_t byte)
{
    if (byte == '\r' || byte == '\n') {
        matcher->matched = 0;
        return false;
    }

    if (byte == (uint8_t)SCREENSHOT_COMMAND[matcher->matched]) {
        matcher->matched++;
        if (matcher->matched == strlen(SCREENSHOT_COMMAND)) {
            matcher->matched = 0;
            return true;
        }
        return false;
    }

    matcher->matched = byte == (uint8_t)SCREENSHOT_COMMAND[0] ? 1 : 0;
    return false;
}
