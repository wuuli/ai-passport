#pragma once

#include <stdbool.h>
#include "duel_sound.h"

bool duel_io_init(bool audio_ready, bool battery_ready);
void duel_io_activate(bool active);
void duel_io_sound(bool enabled, bool music, duel_cue_t cue);
int duel_io_battery(void);
bool duel_io_audio_ready(void);
