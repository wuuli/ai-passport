#pragma once

#include <stdbool.h>
#include "duel_sound.h"
#include "corridor_sound.h"

bool duel_io_init(bool audio_ready, bool battery_ready);
void duel_io_activate(bool active);
void duel_io_sound(bool enabled, bool music, duel_cue_t cue);
/* Non-blocking snapshot; PCM stays in the shared application worker. */
void duel_io_corridor_sound(const ec_sound_scene_t *scene);
int duel_io_battery(void);
bool duel_io_audio_ready(void);
