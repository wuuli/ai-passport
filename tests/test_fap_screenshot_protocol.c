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
    assert(fap_input_parse("FAP_KEY_V1 LEFT")==FAP_INPUT_LEFT);
    assert(fap_input_parse("FAP_KEY_V1 RIGHT")==FAP_INPUT_RIGHT);
    assert(fap_input_parse("FAP_KEY_V1 OK")==FAP_INPUT_OK);
    assert(fap_input_parse("FAP_KEY_V1 BACK")==FAP_INPUT_BACK);
    assert(fap_input_parse("prefix FAP_KEY_V1 OK")==FAP_INPUT_NONE);
    assert(fap_input_parse("FAP_KEY_V1 OKextra")==FAP_INPUT_NONE);
    assert(fap_input_parse(NULL)==FAP_INPUT_NONE);
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
