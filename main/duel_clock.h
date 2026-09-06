#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DUEL_TARGET_MIN_MS UINT32_C(1000)
#define DUEL_TARGET_MAX_MS UINT32_C(6000)
#define DUEL_TARGET_STEP_MS UINT32_C(500)
#define DUEL_TARGET_COUNT ((DUEL_TARGET_MAX_MS - DUEL_TARGET_MIN_MS) / DUEL_TARGET_STEP_MS + 1)
#define DUEL_WINS_REQUIRED 3
#define DUEL_DEBOUNCE_US INT64_C(200000)
#define DUEL_AI_WAIT_US INT64_C(1100000)
#define DUEL_CELEBRATION_US INT64_C(1500000)
#define DUEL_MATCH_WIN_US INT64_C(3500000)

typedef enum {
    DUEL_MODE_DUO,
    DUEL_MODE_AI,
} duel_mode_t;

typedef enum {
    DUEL_PHASE_HOME,
    DUEL_PHASE_TARGET,
    DUEL_PHASE_TIMING,
    DUEL_PHASE_HANDOVER,
    DUEL_PHASE_AI_WAIT,
    DUEL_PHASE_SEALED,
    DUEL_PHASE_ROUND_END,
    DUEL_PHASE_RESULT,
    DUEL_PHASE_MATCH_WIN,
} duel_phase_t;

typedef enum {
    DUEL_EVENT_OK,
    DUEL_EVENT_TICK,
    DUEL_EVENT_HOME,
    DUEL_EVENT_TOGGLE_MODE,
} duel_event_t;

typedef struct {
    duel_mode_t mode;
    duel_phase_t phase;
    uint32_t target_random;
    uint32_t ai_random;
    uint32_t target_ms;
    uint32_t elapsed_ms[2];
    uint32_t pending_ai_ms;
    int64_t started_us;
    int64_t phase_started_us;
    int64_t last_event_us;
    int64_t last_ok_us;
    uint8_t score[2];
    uint8_t round;
    uint8_t starter;
    uint8_t current;
    int8_t winner;
    bool finished[2];
    bool automatic[2];
    bool has_ok;
} duel_clock_t;

typedef struct {
    duel_mode_t mode;
    duel_phase_t phase;
    uint32_t target_ms;
    uint32_t elapsed_ms[2];
    uint32_t error_ms[2];
    uint8_t score[2];
    uint8_t round;
    uint8_t current;
    int8_t winner;
    bool finished[2];
    bool automatic[2];
    bool target_visible;
    bool results_visible;
} duel_view_t;

uint32_t duel_clock_target_ms(uint32_t index);
void duel_clock_init(duel_clock_t *game, duel_mode_t mode, uint32_t seed);
bool duel_clock_handle(duel_clock_t *game, duel_event_t event, int64_t now_us);
duel_view_t duel_clock_view(const duel_clock_t *game);
