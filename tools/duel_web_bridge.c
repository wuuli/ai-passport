#include "duel_clock.h"
#include <stdint.h>
#include <stdbool.h>

static duel_clock_t s_game;
static int s_exited = 0;

int web_init(uint32_t seed, int mode)
{
    duel_clock_init(&s_game, (duel_mode_t)mode, seed);
    s_exited = 0;
    return 1;
}

int web_handle(int event, double now_us)
{
    if (s_exited) {
        if (event == DUEL_EVENT_OK || event == DUEL_EVENT_HOME) {
            s_exited = 0;
            duel_clock_init(&s_game, s_game.mode, s_game.target_random);
            return 1;
        }
        return 0;
    }
    return duel_clock_handle(&s_game, (duel_event_t)event, (int64_t)now_us) ? 1 : 0;
}

int web_key(int button, double now_us)
{
    if (s_exited) {
        if (button == 2) {
            s_exited = 0;
            duel_clock_init(&s_game, s_game.mode, s_game.target_random);
            return 1;
        }
        return 0;
    }
    if (button == 2) {
        return duel_clock_handle(&s_game, DUEL_EVENT_OK, (int64_t)now_us) ? 1 : 0;
    }
    if (s_game.phase == DUEL_PHASE_HOME && button == 0) {
        return duel_clock_handle(&s_game, DUEL_EVENT_TOGGLE_MODE, (int64_t)now_us) ? 1 : 0;
    }
    if (s_game.phase == DUEL_PHASE_HOME && button == 1) {
        return 1;
    }
    return 0;
}

int web_tick(double now_us)
{
    if (s_exited) return 0;
    return duel_clock_handle(&s_game, DUEL_EVENT_TICK, (int64_t)now_us) ? 1 : 0;
}

void web_pause(double now_us)
{
    s_exited = 0;
    duel_clock_handle(&s_game, DUEL_EVENT_HOME, (int64_t)now_us);
}

void web_home(double now_us)
{
    s_exited = 0;
    duel_clock_handle(&s_game, DUEL_EVENT_HOME, (int64_t)now_us);
}

void web_exit(void)
{
    s_exited = 1;
}

int web_set_fixture(int phase, uint32_t target_ms, uint32_t elapsed0, uint32_t elapsed1,
                    int score0, int score1, int starter, int current, int winner,
                    int auto0, int auto1, double now_us)
{
    s_game.phase = (duel_phase_t)phase;
    s_game.target_ms = target_ms;
    s_game.elapsed_ms[0] = elapsed0;
    s_game.elapsed_ms[1] = elapsed1;
    s_game.finished[0] = elapsed0 > 0;
    s_game.finished[1] = elapsed1 > 0;
    s_game.automatic[0] = auto0 != 0;
    s_game.automatic[1] = auto1 != 0;
    s_game.score[0] = (uint8_t)score0;
    s_game.score[1] = (uint8_t)score1;
    s_game.round = (uint8_t)(score0 + score1 + 1);
    s_game.starter = (uint8_t)starter;
    s_game.current = (uint8_t)current;
    s_game.winner = (int8_t)winner;
    s_game.started_us = 0;
    s_game.pending_ai_ms = 0;
    s_game.phase_started_us = (int64_t)now_us;
    s_game.last_event_us = (int64_t)now_us;
    s_game.last_ok_us = (int64_t)now_us;
    s_game.has_ok = true;
    s_exited = 0;
    return 1;
}

uint32_t web_target_ms(uint32_t index)
{
    return duel_clock_target_ms(index);
}

double web_state(unsigned field)
{
    const duel_view_t view = duel_clock_view(&s_game);
    switch (field) {
    case 0: return (double)view.mode;
    case 1: return (double)view.phase;
    case 2: return (double)view.target_ms;
    case 3: return (double)view.elapsed_ms[0];
    case 4: return (double)view.elapsed_ms[1];
    case 5: return (double)view.error_ms[0];
    case 6: return (double)view.error_ms[1];
    case 7: return (double)view.score[0];
    case 8: return (double)view.score[1];
    case 9: return (double)view.round;
    case 10: return (double)s_game.starter;
    case 11: return (double)view.current;
    case 12: return (double)view.winner;
    case 13: return (double)view.finished[0];
    case 14: return (double)view.finished[1];
    case 15: return (double)view.automatic[0];
    case 16: return (double)view.automatic[1];
    case 17: return (double)view.target_visible;
    case 18: return (double)view.results_visible;
    case 19: return (double)s_game.started_us;
    case 20: return (double)s_game.phase_started_us;
    case 21: return (double)s_game.last_event_us;
    case 22: return (double)s_game.last_ok_us;
    case 23: return (double)s_game.target_random;
    case 24: return (double)s_game.ai_random;
    case 25: return (double)s_game.pending_ai_ms;
    case 26: return (double)s_game.has_ok;
    case 27: return (double)s_exited;
    case 28: return (double)s_game.target_ms;
    case 29: return (double)s_game.elapsed_ms[0];
    case 30: return (double)s_game.elapsed_ms[1];
    default: return 0.0;
    }
}
