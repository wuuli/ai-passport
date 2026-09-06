#pragma once

#include "duel_clock.h"

bool duel_ui_create(void);
void duel_ui_set_battery(int battery);
void duel_ui_render(const duel_view_t *view, int battery, bool sound, bool audio_ready,
                    bool input_lost);
void duel_ui_destroy(void);
