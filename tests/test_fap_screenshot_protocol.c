#include "fap_screenshot_protocol.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static unsigned feed(fap_screenshot_matcher_t *matcher, const char *input)
{
    unsigned matches = 0;
    for (size_t index = 0; index < strlen(input); ++index) {
        if (fap_screenshot_matcher_push(matcher, (uint8_t)input[index])) matches++;
    }
    return matches;
}

int main(void)
{
    fap_screenshot_matcher_t matcher;
    fap_screenshot_matcher_reset(&matcher);
    assert(feed(&matcher, "FAP_SCREENSHOT_V1\n") == 1);
    assert(feed(&matcher, "boot log FAP_SCREENSHOT_V1") == 1);
    assert(feed(&matcher, "FAP_SCREEN") == 0);
    assert(feed(&matcher, "SHOT_V1") == 1);
    assert(feed(&matcher, "FAP_SCREEN\nSHOT_V1") == 0);
    assert(feed(&matcher, "FFAP_SCREENSHOT_V1") == 1);
    assert(feed(&matcher, "FAP_SCREENSHOT_V1FAP_SCREENSHOT_V1") == 2);
    assert(feed(&matcher, "FAP_SCREENSHOT_V0") == 0);
    puts("FAP screenshot protocol: fragmented and newline-free commands PASS");
    return 0;
}
