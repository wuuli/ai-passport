#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { EC_TITLE, EC_PLAYING, EC_CLEARED, EC_EXITING } ec_phase_t;
typedef enum { EC_NORMAL, EC_DOOR_MISSING, EC_POSTER_EYES, EC_POSTER_INVERTED,
    EC_EXTRA_VENT, EC_RED_LIGHTS, EC_TALL_NPC, EC_STARING_NPC, EC_ABSENT_NPC } ec_anomaly_t;
typedef enum { EC_LEFT, EC_RIGHT, EC_OK } ec_key_t;
typedef struct {
    unsigned score_before;
    ec_anomaly_t anomaly;
    bool entry_exit, crossed_exit, forward, correct;
} ec_judgement_t;
typedef struct {
    ec_phase_t phase;
    ec_anomaly_t anomaly;
    float x, z, yaw, camera_yaw;
    float npc_x, npc_z, npc_heading, npc_distance, npc_turn;
    int npc_dir;
    int cell;
    unsigned score;
    unsigned hud_score;
    bool hud_score_pending;
    uint32_t rng;
    bool walking, entry_exit, corner_stop;
    int corner_id, bypass_id;
    bool turning;
    bool observing;
    float observe_elapsed,observe_seconds,observe_start_z,observe_target_z;
    float turn_elapsed,turn_start_x,turn_start_z;
    unsigned passages;
    ec_judgement_t last_judgement; /* Debug evidence; never shown during blind play. */
} ec_game_t;

void ec_game_init(ec_game_t *game, uint32_t seed);
void ec_game_key(ec_game_t *game, ec_key_t key);
void ec_game_tick(ec_game_t *game, float seconds);
bool ec_game_walkable(float x, float z);

float ec_game_exit_progress(const ec_game_t *game);
float ec_game_eye_height(const ec_game_t *game);
