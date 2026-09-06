#include "duel_clock.h"

#include <math.h>

static uint32_t random_next(uint32_t *stream)
{
    *stream += UINT32_C(0x9e3779b9);
    uint32_t sample = *stream;
    sample = (sample ^ (sample >> 16)) * UINT32_C(0x21f0aaad);
    sample = (sample ^ (sample >> 15)) * UINT32_C(0x735a2d97);
    return sample ^ (sample >> 15);
}

uint32_t duel_clock_target_ms(uint32_t index)
{
    return index < DUEL_TARGET_COUNT ? 1000 + index * 500 : 0;
}

static uint32_t draw_target(duel_clock_t *game)
{
    const uint32_t threshold = (UINT32_C(0) - DUEL_TARGET_COUNT) % DUEL_TARGET_COUNT;
    uint32_t sample;
    do {
        sample = random_next(&game->target_random);
    } while (sample < threshold);
    return duel_clock_target_ms(sample % DUEL_TARGET_COUNT);
}

static uint32_t draw_ai(duel_clock_t *game)
{
    const double uniform_radius = (random_next(&game->ai_random) + 1.0) / 4294967297.0;
    const double uniform_angle = (random_next(&game->ai_random) + 1.0) / 4294967297.0;
    const double normal = sqrt(-2.0 * log(uniform_radius)) *
                          cos(6.283185307179586 * uniform_angle);
    const double estimate = game->target_ms * (1.0 + 0.12 * normal);
    if (estimate < 220.0) return 220;
    if (estimate > game->target_ms + 2900.0) return game->target_ms + 2900;
    return (uint32_t)lround(estimate);
}

static uint32_t absolute_error(uint32_t elapsed_ms, uint32_t target_ms)
{
    return elapsed_ms > target_ms ? elapsed_ms - target_ms : target_ms - elapsed_ms;
}

static void set_phase(duel_clock_t *game, duel_phase_t phase, int64_t now_us)
{
    game->phase = phase;
    game->phase_started_us = now_us;
}

void duel_clock_init(duel_clock_t *game, duel_mode_t mode, uint32_t seed)
{
    *game = (duel_clock_t){
        .mode = mode == DUEL_MODE_AI ? DUEL_MODE_AI : DUEL_MODE_DUO,
        .phase = DUEL_PHASE_HOME,
        .target_random = seed,
        .ai_random = seed ^ UINT32_C(0x9e3779b9),
        .round = 1,
        .winner = -1,
    };
}

static void return_home(duel_clock_t *game, int64_t now_us)
{
    const duel_mode_t mode = game->mode;
    const uint32_t target_random = game->target_random;
    const uint32_t ai_random = game->ai_random;
    duel_clock_init(game, mode, target_random);
    game->ai_random = ai_random;
    game->last_event_us = now_us;
    game->last_ok_us = now_us;
    game->has_ok = true;
    game->phase_started_us = now_us;
}

static void prepare_round(duel_clock_t *game, bool keep_target, int64_t now_us)
{
    if (!keep_target) game->target_ms = draw_target(game);
    game->current = game->starter;
    game->winner = -1;
    game->started_us = 0;
    game->pending_ai_ms = 0;
    for (unsigned player = 0; player < 2; ++player) {
        game->elapsed_ms[player] = 0;
        game->finished[player] = false;
        game->automatic[player] = false;
    }
    set_phase(game, DUEL_PHASE_TARGET, now_us);
}

static void settle_round(duel_clock_t *game, int64_t now_us)
{
    const uint32_t first_error = absolute_error(game->elapsed_ms[0], game->target_ms);
    const uint32_t second_error = absolute_error(game->elapsed_ms[1], game->target_ms);
    game->winner = first_error == second_error ? -1 : first_error < second_error ? 0 : 1;
    if (game->winner >= 0) ++game->score[(unsigned)game->winner];
    set_phase(game, DUEL_PHASE_ROUND_END, now_us);
}

static void finish_attempt(duel_clock_t *game, uint32_t elapsed_ms,
                           bool automatic, int64_t now_us)
{
    game->elapsed_ms[game->current] = elapsed_ms;
    game->finished[game->current] = true;
    game->automatic[game->current] = automatic;
    if (game->finished[0] && game->finished[1]) {
        set_phase(game, DUEL_PHASE_SEALED, now_us);
    } else {
        game->current = 1 - game->current;
        if (game->mode == DUEL_MODE_AI) {
            game->pending_ai_ms = draw_ai(game);
            set_phase(game, DUEL_PHASE_AI_WAIT, now_us);
        } else {
            set_phase(game, DUEL_PHASE_HANDOVER, now_us);
        }
    }
}

static bool tick(duel_clock_t *game, int64_t now_us)
{
    const int64_t phase_elapsed_us = now_us - game->phase_started_us;
    if (game->phase == DUEL_PHASE_AI_WAIT && phase_elapsed_us >= DUEL_AI_WAIT_US) {
        finish_attempt(game, game->pending_ai_ms, false, now_us);
    } else if (game->phase == DUEL_PHASE_ROUND_END && phase_elapsed_us >= DUEL_CELEBRATION_US) {
        set_phase(game, DUEL_PHASE_RESULT, now_us);
    } else if (game->phase == DUEL_PHASE_MATCH_WIN && phase_elapsed_us >= DUEL_MATCH_WIN_US) {
        return_home(game, now_us);
    } else {
        return false;
    }
    return true;
}

bool duel_clock_handle(duel_clock_t *game, duel_event_t event, int64_t now_us)
{
    if (now_us < 0 || now_us < game->last_event_us ||
        event < DUEL_EVENT_OK || event > DUEL_EVENT_TOGGLE_MODE) return false;
    game->last_event_us = now_us;

    if (event == DUEL_EVENT_HOME) {
        return_home(game, now_us);
        return true;
    }
    if (event == DUEL_EVENT_TOGGLE_MODE) {
        if (game->phase != DUEL_PHASE_HOME) return false;
        game->mode = game->mode == DUEL_MODE_DUO ? DUEL_MODE_AI : DUEL_MODE_DUO;
        return true;
    }
    if (game->phase == DUEL_PHASE_TIMING &&
        now_us - game->started_us >= (int64_t)(game->target_ms + 3000) * 1000) {
        if (event == DUEL_EVENT_OK) {
            game->last_ok_us = now_us;
            game->has_ok = true;
        }
        finish_attempt(game, game->target_ms + 3000, true, now_us);
        return true;
    }
    if (event == DUEL_EVENT_TICK) return tick(game, now_us);
    if (game->has_ok && now_us - game->last_ok_us < DUEL_DEBOUNCE_US) return false;

    switch (game->phase) {
    case DUEL_PHASE_HOME:
        prepare_round(game, false, now_us);
        break;
    case DUEL_PHASE_TARGET:
    case DUEL_PHASE_HANDOVER:
        game->started_us = now_us;
        set_phase(game, DUEL_PHASE_TIMING, now_us);
        break;
    case DUEL_PHASE_TIMING:
        finish_attempt(game, (uint32_t)((now_us - game->started_us + 500) / 1000),
                       false, now_us);
        break;
    case DUEL_PHASE_SEALED:
        settle_round(game, now_us);
        break;
    case DUEL_PHASE_ROUND_END:
        set_phase(game, DUEL_PHASE_RESULT, now_us);
        break;
    case DUEL_PHASE_RESULT:
        if (game->score[0] == DUEL_WINS_REQUIRED || game->score[1] == DUEL_WINS_REQUIRED) {
            set_phase(game, DUEL_PHASE_MATCH_WIN, now_us);
        } else {
            const bool tied = game->winner < 0;
            game->starter = game->mode == DUEL_MODE_AI ? 0 : 1 - game->starter;
            game->round = game->score[0] + game->score[1] + 1;
            prepare_round(game, tied, now_us);
        }
        break;
    case DUEL_PHASE_MATCH_WIN:
        return_home(game, now_us);
        break;
    default:
        return false;
    }
    game->last_ok_us = now_us;
    game->has_ok = true;
    return true;
}

duel_view_t duel_clock_view(const duel_clock_t *game)
{
    duel_view_t view = {
        .mode = game->mode,
        .phase = game->phase,
        .score = {game->score[0], game->score[1]},
        .round = game->round,
        .current = game->current,
        .winner = -1,
        .finished = {game->finished[0], game->finished[1]},
        .automatic = {game->automatic[0], game->automatic[1]},
        .target_visible = game->phase == DUEL_PHASE_TARGET || game->phase == DUEL_PHASE_HANDOVER ||
                          game->phase >= DUEL_PHASE_SEALED,
        .results_visible = game->phase == DUEL_PHASE_RESULT || game->phase == DUEL_PHASE_MATCH_WIN,
    };
    if (view.target_visible) view.target_ms = game->target_ms;
    if (game->phase == DUEL_PHASE_ROUND_END) view.winner = game->winner;
    if (view.results_visible) {
        view.winner = game->winner;
        for (unsigned player = 0; player < 2; ++player) {
            view.elapsed_ms[player] = game->elapsed_ms[player];
            view.error_ms[player] = absolute_error(game->elapsed_ms[player], game->target_ms);
        }
    }
    return view;
}
