#pragma once
#include "corridor_game.h"
#include <stddef.h>

#define EC_SOUND_RATE 16000

/* Cumulative events survive the native worker's overwrite queue. */
typedef struct {
    bool enabled, ambient;
    unsigned steps, clears;
} ec_sound_scene_t;
typedef struct {
    ec_sound_scene_t scene;
    float x, z, distance;
    ec_phase_t phase;
    unsigned passages;
    bool walking, observing;
} ec_sound_tracker_t;
typedef struct {
    ec_sound_scene_t scene;
    uint32_t noise;
    int32_t air, shoe;
    unsigned hum_phase, clear_phase;
    unsigned ambience, step_left, clear_left;
} ec_sound_t;

void ec_sound_track_init(ec_sound_tracker_t *tracker, const ec_game_t *game);
void ec_sound_track(ec_sound_tracker_t *tracker, const ec_game_t *game);
void ec_sound_set(ec_sound_t *sound, const ec_sound_scene_t *scene);
bool ec_sound_running(const ec_sound_t *sound);
void ec_sound_render(ec_sound_t *sound, int16_t *samples, size_t count);
/* Device output calibration: browser slider 60% at maximum gain 10.
 * Apply once after rendering; the browser applies its own playback gain. */
void ec_sound_device_gain(int16_t *samples, size_t count);
