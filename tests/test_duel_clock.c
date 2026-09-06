#include "duel_clock.h"

#include <assert.h>
#include <stdio.h>

typedef struct {
    duel_clock_t game;
    int64_t now_us;
} fixture_t;

static fixture_t fixture(duel_mode_t mode, uint32_t seed)
{
    fixture_t test = {0};
    duel_clock_init(&test.game, mode, seed);
    return test;
}

static bool send_after(fixture_t *test, duel_event_t event, int64_t delay_us)
{
    test->now_us += delay_us;
    return duel_clock_handle(&test->game, event, test->now_us);
}

static void press(fixture_t *test)
{
    assert(send_after(test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US));
}

static void assert_hidden(const duel_clock_t *game, bool target_visible)
{
    const duel_view_t view = duel_clock_view(game);
    assert(!view.results_visible);
    assert(view.winner == -1);
    assert(view.target_visible == target_visible);
    assert(view.target_ms == (target_visible ? game->target_ms : 0));
    for (unsigned player = 0; player < 2; ++player) {
        assert(view.elapsed_ms[player] == 0);
        assert(view.error_ms[player] == 0);
    }
}

static void play_attempt(fixture_t *test, uint32_t elapsed_ms)
{
    assert(test->game.phase == DUEL_PHASE_TARGET || test->game.phase == DUEL_PHASE_HANDOVER);
    press(test);
    assert(test->game.phase == DUEL_PHASE_TIMING);
    assert_hidden(&test->game, false);
    assert(send_after(test, DUEL_EVENT_OK, (int64_t)elapsed_ms * 1000));
}

static void play_duo_round(fixture_t *test, int winner)
{
    if (test->game.phase == DUEL_PHASE_HOME) press(test);
    const uint32_t target_ms = test->game.target_ms;
    const uint8_t first = test->game.current;
    const uint8_t score_before[2] = {test->game.score[0], test->game.score[1]};
    const uint32_t estimates[2] = {
        winner < 0 ? target_ms - 20 : target_ms + (winner == 0 ? 0 : 100),
        winner < 0 ? target_ms + 20 : target_ms + (winner == 1 ? 0 : 100),
    };
    play_attempt(test, estimates[first]);
    assert(test->game.phase == DUEL_PHASE_HANDOVER);
    assert(test->game.current == 1 - first);
    assert_hidden(&test->game, true);
    play_attempt(test, estimates[1 - first]);
    assert(test->game.phase == DUEL_PHASE_SEALED);
    assert_hidden(&test->game, true);
    assert(test->game.winner == -1);
    assert(test->game.elapsed_ms[0] == estimates[0]);
    assert(test->game.elapsed_ms[1] == estimates[1]);
    const duel_view_t view = duel_clock_view(&test->game);
    assert(view.score[0] == score_before[0] && view.score[1] == score_before[1]);
}

static void show_result(fixture_t *test)
{
    assert(test->game.phase == DUEL_PHASE_SEALED);
    const uint8_t score_before[2] = {test->game.score[0], test->game.score[1]};
    assert(!send_after(test, DUEL_EVENT_TICK, INT64_C(60000000)));
    assert(test->game.phase == DUEL_PHASE_SEALED);
    assert_hidden(&test->game, true);
    assert(test->game.score[0] == score_before[0] && test->game.score[1] == score_before[1]);
    press(test);
    assert(test->game.phase == DUEL_PHASE_ROUND_END);
    const duel_view_t celebration = duel_clock_view(&test->game);
    assert(!celebration.results_visible && celebration.winner == test->game.winner);
    assert(celebration.elapsed_ms[0] == 0 && celebration.elapsed_ms[1] == 0);
    assert(celebration.error_ms[0] == 0 && celebration.error_ms[1] == 0);
    assert(!send_after(test, DUEL_EVENT_TICK, DUEL_CELEBRATION_US - 1));
    assert(send_after(test, DUEL_EVENT_TICK, 1));
    assert(test->game.phase == DUEL_PHASE_RESULT);
    const duel_view_t view = duel_clock_view(&test->game);
    assert(view.results_visible && view.target_visible);
    for (unsigned player = 0; player < 2; ++player) {
        const uint32_t elapsed = test->game.elapsed_ms[player];
        const uint32_t target = test->game.target_ms;
        assert(view.elapsed_ms[player] == elapsed);
        assert(view.error_ms[player] == (elapsed > target ? elapsed - target : target - elapsed));
    }
    assert(!send_after(test, DUEL_EVENT_TICK, INT64_C(60000000)));
    assert(test->game.phase == DUEL_PHASE_RESULT);
}

static void test_initial_state_and_mode(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 0);
    assert(sizeof(test.game) <= 128);
    assert(test.game.phase == DUEL_PHASE_HOME);
    assert(test.game.round == 1 && test.game.current == 0);
    assert_hidden(&test.game, false);
    assert(send_after(&test, DUEL_EVENT_TOGGLE_MODE, 0));
    assert(test.game.mode == DUEL_MODE_AI);
    assert(send_after(&test, DUEL_EVENT_TOGGLE_MODE, 0));
    assert(test.game.mode == DUEL_MODE_DUO);
    press(&test);
    assert(!send_after(&test, DUEL_EVENT_TOGGLE_MODE, 0));
    assert(test.game.mode == DUEL_MODE_DUO);
}

static void test_all_targets_and_winners(void)
{
    unsigned seen = 0;
    for (uint32_t seed = 0; seed < 1000; ++seed) {
        fixture_t test = fixture(DUEL_MODE_DUO, seed);
        press(&test);
        const uint32_t target = test.game.target_ms;
        assert(target >= DUEL_TARGET_MIN_MS && target <= DUEL_TARGET_MAX_MS &&
               (target - DUEL_TARGET_MIN_MS) % DUEL_TARGET_STEP_MS == 0);
        const unsigned index = (target - DUEL_TARGET_MIN_MS) / DUEL_TARGET_STEP_MS;
        assert(target == duel_clock_target_ms(index));
        seen |= 1u << index;
        play_duo_round(&test, (int)(seed % 2));
        show_result(&test);
        const duel_view_t view = duel_clock_view(&test.game);
        assert(view.winner == (int)(seed % 2));
        assert(view.error_ms[seed % 2] == 0 && view.error_ms[1 - seed % 2] == 100);
        assert(test.game.score[seed % 2] == 1);
        assert(test.game.score[1 - seed % 2] == 0);
    }
    assert(seen == (UINT32_C(1) << DUEL_TARGET_COUNT) - 1);
    assert(duel_clock_target_ms(DUEL_TARGET_COUNT) == 0);
    assert(duel_clock_target_ms(UINT32_MAX) == 0);
}

static void test_debounce_and_rounding(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 10);
    assert(send_after(&test, DUEL_EVENT_OK, 0));
    assert(!send_after(&test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
    assert(test.game.phase == DUEL_PHASE_TARGET);
    assert(send_after(&test, DUEL_EVENT_OK, 1));
    assert(test.game.phase == DUEL_PHASE_TIMING);
    assert(!send_after(&test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
    assert(send_after(&test, DUEL_EVENT_OK, 1));
    assert(test.game.elapsed_ms[0] == 200);
    assert(!send_after(&test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
    assert(test.game.phase == DUEL_PHASE_HANDOVER);
    assert(send_after(&test, DUEL_EVENT_OK, 1));
    assert(test.game.phase == DUEL_PHASE_TIMING);
    assert(send_after(&test, DUEL_EVENT_OK, 1500499));
    assert(test.game.elapsed_ms[1] == 1500);

    test = fixture(DUEL_MODE_DUO, 10);
    press(&test);
    press(&test);
    assert(send_after(&test, DUEL_EVENT_OK, 1500500));
    assert(test.game.elapsed_ms[0] == 1501);
}

static void test_stale_and_invalid_events(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 1);
    assert(!duel_clock_handle(&test.game, DUEL_EVENT_OK, -1));
    assert(!duel_clock_handle(&test.game, (duel_event_t)99, 100));
    assert(test.game.last_event_us == 0);
    press(&test);
    press(&test);
    assert(!send_after(&test, DUEL_EVENT_TICK, 500000));
    assert(!duel_clock_handle(&test.game, DUEL_EVENT_OK, test.now_us - 1));
    assert(!test.game.finished[0]);
    assert(send_after(&test, DUEL_EVENT_OK, 500000));
    assert(test.game.elapsed_ms[0] == 1000);

    test = fixture(DUEL_MODE_DUO, 1);
    press(&test);
    press(&test);
    const int64_t batch_time_us = test.now_us + 1000000;
    assert(duel_clock_handle(&test.game, DUEL_EVENT_OK, batch_time_us + 1));
    assert(!duel_clock_handle(&test.game, DUEL_EVENT_TICK, batch_time_us));
    assert(test.game.elapsed_ms[0] == 1000);
    assert(test.game.phase == DUEL_PHASE_HANDOVER);
    const int64_t next_batch_us = batch_time_us + DUEL_DEBOUNCE_US + 1;
    assert(!duel_clock_handle(&test.game, DUEL_EVENT_TICK, next_batch_us));
    assert(duel_clock_handle(&test.game, DUEL_EVENT_OK, next_batch_us + 1));
    assert(test.game.phase == DUEL_PHASE_TIMING);
    assert(duel_clock_handle(&test.game, DUEL_EVENT_OK, next_batch_us + 1000001));
    assert(test.game.elapsed_ms[1] == 1000);
}

static void test_timeout_boundaries(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 1);
    press(&test);
    press(&test);
    const uint32_t limit_ms = test.game.target_ms + 3000;
    assert(!send_after(&test, DUEL_EVENT_TICK, (int64_t)limit_ms * 1000 - 1));
    assert(send_after(&test, DUEL_EVENT_TICK, 1));
    assert(test.game.phase == DUEL_PHASE_HANDOVER);
    assert(test.game.elapsed_ms[0] == limit_ms && test.game.automatic[0]);
    assert_hidden(&test.game, true);
    press(&test);
    assert(send_after(&test, DUEL_EVENT_TICK, INT64_C(3600000000)));
    assert(test.game.elapsed_ms[1] == limit_ms && test.game.automatic[1]);
    assert(test.game.winner == -1);
    assert(test.game.score[0] == 0 && test.game.score[1] == 0);
    assert_hidden(&test.game, true);
    show_result(&test);
    const duel_view_t view = duel_clock_view(&test.game);
    assert(view.error_ms[0] == 3000 && view.error_ms[1] == 3000);
    assert(view.automatic[0] && view.automatic[1]);
}

static void test_late_ok_only_stops(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 1);
    press(&test);
    press(&test);
    assert(send_after(&test, DUEL_EVENT_OK, (int64_t)(test.game.target_ms + 3000) * 1000));
    assert(test.game.phase == DUEL_PHASE_HANDOVER);
    assert(test.game.automatic[0]);
    assert(!send_after(&test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
    assert(send_after(&test, DUEL_EVENT_OK, 1));
    assert(test.game.phase == DUEL_PHASE_TIMING);
    assert(send_after(&test, DUEL_EVENT_OK, INT64_C(90000000)));
    assert(test.game.phase == DUEL_PHASE_SEALED);
    assert(test.game.automatic[1]);
}

static void test_last_microsecond_before_timeout(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 2);
    press(&test);
    press(&test);
    const uint32_t limit_ms = test.game.target_ms + 3000;
    assert(send_after(&test, DUEL_EVENT_OK, (int64_t)limit_ms * 1000 - 1));
    assert(test.game.elapsed_ms[0] == limit_ms);
    assert(!test.game.automatic[0]);
}

static void test_ties_repeat_without_overflow(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 9);
    press(&test);
    const uint32_t target_ms = test.game.target_ms;
    const uint32_t target_random = test.game.target_random;
    for (unsigned attempt = 0; attempt < 1000; ++attempt) {
        assert(test.game.starter == attempt % 2);
        play_duo_round(&test, -1);
        show_result(&test);
        const duel_view_t view = duel_clock_view(&test.game);
        assert(view.winner == -1 && view.error_ms[0] == 20 && view.error_ms[1] == 20);
        press(&test);
        assert(test.game.phase == DUEL_PHASE_TARGET);
        assert(test.game.target_ms == target_ms);
        assert(test.game.target_random == target_random);
        assert(test.game.round == 1);
        assert(test.game.score[0] == 0 && test.game.score[1] == 0);
        assert(!test.game.finished[0] && !test.game.finished[1]);
    }
}

static void test_round_results_require_confirmation(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 8);
    play_duo_round(&test, 0);
    assert(!send_after(&test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
    assert(test.game.phase == DUEL_PHASE_SEALED);
    assert(!send_after(&test, DUEL_EVENT_TICK, INT64_C(3600000000)));
    assert(test.game.phase == DUEL_PHASE_SEALED);
    assert_hidden(&test.game, true);
    assert(test.game.score[0] == 0 && test.game.score[1] == 0);
    press(&test);
    assert(test.game.phase == DUEL_PHASE_ROUND_END);
    assert(duel_clock_view(&test.game).winner == 0);
    assert(!send_after(&test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
    assert(!duel_clock_view(&test.game).results_visible);
    assert(send_after(&test, DUEL_EVENT_TICK, DUEL_CELEBRATION_US - DUEL_DEBOUNCE_US + 1));
    assert(test.game.phase == DUEL_PHASE_RESULT);
    assert(!send_after(&test, DUEL_EVENT_TICK, INT64_C(3600000000)));
    assert(test.game.phase == DUEL_PHASE_RESULT);
    assert(test.game.score[0] == 1);
    assert(test.game.round == 1);
    assert(!send_after(&test, DUEL_EVENT_TICK, 1));
    press(&test);
    assert(test.game.phase == DUEL_PHASE_TARGET && test.game.round == 2);

    test = fixture(DUEL_MODE_DUO, 8);
    play_duo_round(&test, 1);
    assert(!send_after(&test, DUEL_EVENT_TICK, INT64_C(10000000)));
    assert(test.game.phase == DUEL_PHASE_SEALED);
    press(&test);
    assert(test.game.phase == DUEL_PHASE_ROUND_END);
    press(&test);
    assert(test.game.phase == DUEL_PHASE_RESULT);
    assert(test.game.score[1] == 1);
}

static void test_all_match_paths_and_rematch(void)
{
    for (unsigned sequence = 0; sequence < 32; ++sequence) {
        fixture_t test = fixture(DUEL_MODE_DUO, sequence);
        for (unsigned round = 0; round < 5; ++round) {
            const unsigned winner = (sequence >> round) & 1;
            assert(test.game.round == round + 1);
            assert(test.game.starter == round % 2);
            play_duo_round(&test, (int)winner);
            assert(test.game.score[0] + test.game.score[1] == round);
            show_result(&test);
            assert(test.game.score[0] + test.game.score[1] == round + 1);
            const bool complete = test.game.score[winner] == DUEL_WINS_REQUIRED;
            press(&test);
            if (complete) {
                assert(test.game.phase == DUEL_PHASE_MATCH_WIN);
                assert(test.game.winner == (int)winner);
                const uint32_t target_random = test.game.target_random;
                assert(!send_after(&test, DUEL_EVENT_TICK, DUEL_MATCH_WIN_US - 1));
                assert(test.game.score[winner] == DUEL_WINS_REQUIRED);
                if (sequence % 2 == 0) press(&test);
                else assert(send_after(&test, DUEL_EVENT_TICK, 1));
                assert(test.game.phase == DUEL_PHASE_HOME);
                assert(test.game.target_random == target_random);
                assert_hidden(&test.game, false);
                assert(!send_after(&test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
                assert(test.game.phase == DUEL_PHASE_HOME);
                assert(send_after(&test, DUEL_EVENT_OK, 1));
                assert(test.game.phase == DUEL_PHASE_TARGET);
                assert(test.game.round == 1 && test.game.starter == 0);
                assert(test.game.score[0] == 0 && test.game.score[1] == 0);
                assert(test.game.target_random != target_random);
                assert_hidden(&test.game, true);
                break;
            }
            assert(round < 4);
            assert(test.game.phase == DUEL_PHASE_TARGET);
        }
    }
}

static uint32_t ai_round(fixture_t *test, uint32_t human_ms)
{
    if (test->game.phase == DUEL_PHASE_HOME) press(test);
    assert(test->game.current == 0);
    play_attempt(test, human_ms);
    assert(test->game.phase == DUEL_PHASE_AI_WAIT);
    assert_hidden(&test->game, false);
    assert(!send_after(test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US));
    assert(!send_after(test, DUEL_EVENT_TICK, DUEL_AI_WAIT_US - DUEL_DEBOUNCE_US - 1));
    assert(send_after(test, DUEL_EVENT_TICK, 1));
    assert(test->game.phase == DUEL_PHASE_SEALED);
    assert_hidden(&test->game, true);
    assert(!test->game.automatic[1]);
    assert(test->game.elapsed_ms[1] >= 220);
    assert(test->game.elapsed_ms[1] <= test->game.target_ms + 2900);
    return test->game.elapsed_ms[1];
}

static void test_ai_determinism_independence_and_bounds(void)
{
    for (uint32_t seed = 0; seed < 5000; ++seed) {
        fixture_t fast = fixture(DUEL_MODE_AI, seed);
        fixture_t slow = fixture(DUEL_MODE_AI, seed);
        const uint32_t fast_ai = ai_round(&fast, 500);
        const uint32_t slow_ai = ai_round(&slow, 3900);
        assert(fast.game.target_ms == slow.game.target_ms);
        assert(fast_ai == slow_ai);
        assert(fast.game.target_random == slow.game.target_random);
        assert(fast.game.ai_random == slow.game.ai_random);
    }
}

static void test_ai_always_human_first(void)
{
    fixture_t test = fixture(DUEL_MODE_AI, UINT32_MAX);
    for (unsigned round = 0; round < 5; ++round) {
        ai_round(&test, 200);
        show_result(&test);
        press(&test);
        if (test.game.phase == DUEL_PHASE_MATCH_WIN) break;
        assert(test.game.phase == DUEL_PHASE_TARGET);
        assert(test.game.current == 0 && test.game.starter == 0);
    }
    assert(test.game.phase == DUEL_PHASE_MATCH_WIN);
    press(&test);
    assert(test.game.phase == DUEL_PHASE_HOME);
    assert(test.game.mode == DUEL_MODE_AI && test.game.current == 0);
}

static void test_ai_after_human_timeout(void)
{
    fixture_t test = fixture(DUEL_MODE_AI, 42);
    press(&test);
    press(&test);
    assert(send_after(&test, DUEL_EVENT_TICK,
                      (int64_t)(test.game.target_ms + 3000) * 1000));
    assert(test.game.phase == DUEL_PHASE_AI_WAIT);
    assert(test.game.automatic[0]);
    assert_hidden(&test.game, false);
    assert(send_after(&test, DUEL_EVENT_TICK, DUEL_AI_WAIT_US));
    assert(test.game.phase == DUEL_PHASE_SEALED);
    assert_hidden(&test.game, true);
    assert(test.game.winner == -1);
    show_result(&test);
    assert(test.game.winner == 1);
}

static void assert_cancelled(fixture_t *test)
{
    const uint32_t target_random = test->game.target_random;
    const uint32_t ai_random = test->game.ai_random;
    const duel_mode_t mode = test->game.mode;
    assert(send_after(test, DUEL_EVENT_HOME, 1000));
    assert(test->game.phase == DUEL_PHASE_HOME);
    assert(test->game.score[0] == 0 && test->game.score[1] == 0);
    assert(test->game.round == 1 && test->game.starter == 0);
    assert(test->game.target_random == target_random && test->game.ai_random == ai_random);
    assert(test->game.mode == mode);
    assert_hidden(&test->game, false);
    assert(!duel_clock_handle(&test->game, DUEL_EVENT_OK, test->now_us - 1));
    assert(!send_after(test, DUEL_EVENT_OK, DUEL_DEBOUNCE_US - 1));
    assert(!send_after(test, DUEL_EVENT_TICK, INT64_C(10000000)));
    assert(test->game.phase == DUEL_PHASE_HOME);
    press(test);
    assert(test->game.phase == DUEL_PHASE_TARGET);
    assert(!test->game.finished[0] && !test->game.finished[1]);
}

static void test_cancel_every_phase(void)
{
    fixture_t home = fixture(DUEL_MODE_DUO, 12);
    assert_cancelled(&home);
    fixture_t target = fixture(DUEL_MODE_DUO, 12);
    press(&target);
    fixture_t timing = target;
    press(&timing);
    fixture_t handover = timing;
    assert(send_after(&handover, DUEL_EVENT_OK, INT64_C(1000000)));
    fixture_t second = handover;
    press(&second);
    fixture_t sealed = second;
    assert(send_after(&sealed, DUEL_EVENT_OK, INT64_C(2000000)));
    fixture_t round_end = sealed;
    press(&round_end);
    fixture_t result = round_end;
    press(&result);
    fixture_t ai = fixture(DUEL_MODE_AI, 12);
    press(&ai);
    play_attempt(&ai, 1000);
    fixture_t final = fixture(DUEL_MODE_DUO, 12);
    for (unsigned round = 0; round < 3; ++round) {
        play_duo_round(&final, 0);
        show_result(&final);
        press(&final);
    }
    assert(final.game.phase == DUEL_PHASE_MATCH_WIN);
    fixture_t *states[] = {&target, &timing, &handover, &second, &sealed,
                          &round_end, &result, &ai, &final};
    for (unsigned index = 0; index < sizeof(states) / sizeof(states[0]); ++index) {
        assert_cancelled(states[index]);
    }
}

static void test_large_monotonic_timestamp(void)
{
    fixture_t test = fixture(DUEL_MODE_DUO, 1);
    test.now_us = INT64_MAX - INT64_C(20000000);
    press(&test);
    press(&test);
    assert(duel_clock_handle(&test.game, DUEL_EVENT_TICK, INT64_MAX));
    assert(test.game.phase == DUEL_PHASE_HANDOVER);
    assert(test.game.elapsed_ms[0] == test.game.target_ms + 3000);
}

int main(void)
{
    test_initial_state_and_mode();
    test_all_targets_and_winners();
    test_debounce_and_rounding();
    test_stale_and_invalid_events();
    test_timeout_boundaries();
    test_late_ok_only_stops();
    test_last_microsecond_before_timeout();
    test_ties_repeat_without_overflow();
    test_round_results_require_confirmation();
    test_all_match_paths_and_rematch();
    test_ai_determinism_independence_and_bounds();
    test_ai_always_human_first();
    test_ai_after_human_timeout();
    test_cancel_every_phase();
    test_large_monotonic_timestamp();
    printf("Time Challenge: 15 test groups PASS (model: %zu bytes)\n", sizeof(duel_clock_t));
    return 0;
}
