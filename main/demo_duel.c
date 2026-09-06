#include "demo.h"
#include "duel_clock.h"
#include "duel_io.h"
#include "duel_ui.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"

static duel_clock_t s_game;
static bool s_visible;
static bool s_sound = true;
static bool s_input_lost;
static bool s_dimmed;
static int64_t s_last_activity_us;

static void sound(duel_cue_t cue)
{
    duel_io_sound(s_sound && !s_dimmed, s_game.phase != DUEL_PHASE_TIMING, cue);
}

static void render(void)
{
    if (!s_visible) return;
    const duel_view_t view = duel_clock_view(&s_game);
    duel_ui_render(&view, duel_io_battery(), s_sound, duel_io_audio_ready(), s_input_lost);
}

static void update(duel_event_t event, int64_t now_us)
{
    const duel_phase_t previous = s_game.phase;
    if (!duel_clock_handle(&s_game, event, now_us)) return;
    duel_cue_t cue = event == DUEL_EVENT_OK || event == DUEL_EVENT_TOGGLE_MODE ? DUEL_CUE_BUTTON : DUEL_CUE_NONE;
    if (s_game.phase == DUEL_PHASE_TIMING) cue = DUEL_CUE_START;
    else if (previous == DUEL_PHASE_TIMING) cue = DUEL_CUE_STOP;
    else if (s_game.phase == DUEL_PHASE_MATCH_WIN) cue = DUEL_CUE_MATCH;
    else if (s_game.phase == DUEL_PHASE_ROUND_END && s_game.winner >= 0) {
        cue = s_game.mode == DUEL_MODE_AI && s_game.winner == 1 ? DUEL_CUE_LOSE : DUEL_CUE_WIN;
    }
    sound(cue);
    if (s_game.phase == DUEL_PHASE_TIMING || previous == DUEL_PHASE_TIMING) {
        ESP_LOGI("duel", "phase=%d round=%u player=%u", s_game.phase,
                 (unsigned)s_game.round, (unsigned)s_game.current);
    }
    render();
}

void demo_duel_enter(void)
{
    duel_clock_init(&s_game, DUEL_MODE_DUO, esp_random());
    s_input_lost = false;
    s_dimmed = false;
    s_last_activity_us = esp_timer_get_time();
    s_visible = duel_ui_create();
    duel_io_activate(true);
    sound(DUEL_CUE_NONE);
    render();
    ESP_LOGI("duel", "Time Challenge ready; OK starts, UP selects mode, DOWN toggles sound");
}

void demo_duel_exit(void)
{
    duel_io_activate(false);
    s_visible = false;
    duel_ui_destroy();
    if (s_dimmed) bsp_display_backlight(100);
    s_dimmed = false;
}

void demo_duel_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    demo_duel_key_at(button, event, esp_timer_get_time());
}

void demo_duel_key_at(bsp_btn_t button, bsp_btn_ev_t event, int64_t timestamp_us)
{
    if (event != BSP_BTN_PRESS || !s_visible) return;
    s_last_activity_us = timestamp_us;
    if (s_dimmed) {
        bsp_display_backlight(100);
        s_dimmed = false;
        sound(DUEL_CUE_BUTTON);
        return;
    }
    if (button == BSP_BTN_OK) {
        s_input_lost = false;
        update(DUEL_EVENT_OK, timestamp_us);
    } else if (s_game.phase == DUEL_PHASE_HOME && button == BSP_BTN_UP) {
        update(DUEL_EVENT_TOGGLE_MODE, timestamp_us);
    } else if (s_game.phase == DUEL_PHASE_HOME && button == BSP_BTN_DOWN) {
        s_sound = duel_io_audio_ready() && !s_sound;
        sound(DUEL_CUE_BUTTON);
        render();
    }
}

void demo_duel_tick(int64_t now_us)
{
    if (!s_visible) return;
    update(DUEL_EVENT_TICK, now_us);
    duel_ui_set_battery(duel_io_battery());
    if (!s_dimmed && s_game.phase != DUEL_PHASE_TIMING &&
        now_us - s_last_activity_us >= INT64_C(60000000)) {
        bsp_display_backlight(15);
        s_dimmed = true;
        sound(DUEL_CUE_NONE);
    }
}

void demo_duel_input_lost(int64_t now_us)
{
    s_input_lost = true;
    duel_io_activate(true);
    update(DUEL_EVENT_HOME, now_us);
}
