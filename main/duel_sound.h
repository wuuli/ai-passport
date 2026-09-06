#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DUEL_SOUND_RATE 16000

typedef enum {
    DUEL_CUE_NONE,
    DUEL_CUE_BUTTON,
    DUEL_CUE_START,
    DUEL_CUE_STOP,
    DUEL_CUE_WIN,
    DUEL_CUE_LOSE,
    DUEL_CUE_MATCH,
} duel_cue_t;

typedef struct {
    bool enabled;
    bool music;
    duel_cue_t cue;
    unsigned music_sample;
    unsigned melody_phase;
    unsigned bass_phase;
    unsigned cue_note;
    unsigned cue_sample;
    unsigned cue_phase;
} duel_sound_t;

void duel_sound_set(duel_sound_t *sound, bool enabled, bool music, duel_cue_t cue);
bool duel_sound_running(const duel_sound_t *sound);
void duel_sound_render(duel_sound_t *sound, int16_t *samples, size_t count);
