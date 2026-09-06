#include "demo.h"
#include "duel_io.h"
#include "duel_ui.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int s_cached_battery;
static int s_shown_battery;
static unsigned s_renders;
static unsigned s_battery_updates;
static unsigned s_sounds;
static uint8_t s_brightness;
static bool s_ui_alive;
static bool s_active;
static int64_t s_now_us;
static duel_view_t s_view;

int64_t esp_timer_get_time(void) { return s_now_us; }
uint32_t esp_random(void) { return 123; }
void bsp_display_backlight(uint8_t percent) { s_brightness = percent; }
void duel_io_activate(bool active) { s_active = active; }
int duel_io_battery(void) { return s_cached_battery; }
bool duel_io_audio_ready(void) { return true; }
void duel_io_sound(bool enabled, bool music, duel_cue_t cue)
{
    (void)enabled;
    (void)music;
    (void)cue;
    ++s_sounds;
}
bool duel_ui_create(void) { s_ui_alive = true; return true; }
void duel_ui_destroy(void) { s_ui_alive = false; }
void duel_ui_set_battery(int battery)
{
    assert(s_ui_alive);
    s_shown_battery = battery;
    ++s_battery_updates;
}
void duel_ui_render(const duel_view_t *view, int battery, bool sound,
                    bool audio_ready, bool input_lost)
{
    (void)sound;
    (void)audio_ready;
    (void)input_lost;
    assert(s_ui_alive);
    s_view = *view;
    s_shown_battery = battery;
    ++s_renders;
}

static void enter(int battery)
{
    s_cached_battery = battery;
    s_shown_battery = -999;
    s_renders = s_battery_updates = s_sounds = 0;
    s_brightness = 100;
    s_now_us = 0;
    demo_duel_enter();
    assert(s_active && s_ui_alive && s_view.phase == DUEL_PHASE_HOME);
}

static void key(int64_t now_us)
{
    s_now_us = now_us;
    demo_duel_key_at(BSP_BTN_OK, BSP_BTN_PRESS, now_us);
}

static void battery_tick(int battery, int64_t now_us)
{
    const unsigned renders = s_renders;
    const unsigned sounds = s_sounds;
    s_cached_battery = battery;
    s_now_us = now_us;
    demo_duel_tick(now_us);
    if (s_shown_battery != battery) {
        fprintf(stderr, "Battery refresh failed: cache=%d screen=%d phase=%d\n",
                battery, s_shown_battery, s_view.phase);
        exit(1);
    }
    assert(s_renders == renders);
    assert(s_sounds == sounds);
}

static void test_delayed_first_read(void)
{
    enter(-1);
    assert(s_shown_battery == -1 && s_renders == 1);
    battery_tick(67, 10000);
    for (int64_t now_us = 20000; now_us <= 10000000; now_us += 10000) battery_tick(67, now_us);
    battery_tick(66, 11000000);
    battery_tick(-1, 12000000);
    battery_tick(65, 13000000);
    battery_tick(0, 14000000);
    battery_tick(100, 15000000);
    demo_duel_exit();
}

static void test_timing_and_celebration_unaffected(void)
{
    enter(67);
    key(200000);
    const uint32_t target_ms = s_view.target_ms;
    key(400000);
    assert(s_view.phase == DUEL_PHASE_TIMING);
    battery_tick(66, 600000);
    const int64_t first_stop = 400000 + (int64_t)target_ms * 1000;
    key(first_stop);
    assert(s_view.phase == DUEL_PHASE_HANDOVER);
    battery_tick(65, first_stop + 100000);
    key(first_stop + 300000);
    const int64_t second_stop = first_stop + 300000 + (int64_t)(target_ms + 100) * 1000;
    key(second_stop);
    assert(s_view.phase == DUEL_PHASE_SEALED);
    battery_tick(64, second_stop + 100000);
    key(second_stop + 300000);
    assert(s_view.phase == DUEL_PHASE_ROUND_END && s_view.winner == 0);
    battery_tick(63, second_stop + 400000);
    battery_tick(62, second_stop + 1799999);
    demo_duel_tick(second_stop + 1800000);
    assert(s_view.phase == DUEL_PHASE_RESULT);
    assert(s_view.elapsed_ms[0] == target_ms && s_view.elapsed_ms[1] == target_ms + 100);
    assert(s_view.score[0] == 1 && s_view.score[1] == 0);
    battery_tick(61, second_stop + 1900000);
    demo_duel_exit();
}

static void test_idle_exit_and_reentry(void)
{
    enter(67);
    demo_duel_tick(60000000);
    assert(s_brightness == 15);
    battery_tick(66, 61000000);
    key(62000000);
    assert(s_brightness == 100 && s_view.phase == DUEL_PHASE_HOME);
    demo_duel_exit();
    const unsigned updates = s_battery_updates;
    const unsigned renders = s_renders;
    const unsigned sounds = s_sounds;
    s_cached_battery = 40;
    demo_duel_tick(63000000);
    assert(!s_active && !s_ui_alive && s_battery_updates == updates);
    assert(s_renders == renders && s_sounds == sounds);
    s_now_us = 64000000;
    demo_duel_enter();
    assert(s_shown_battery == 40 && s_view.phase == DUEL_PHASE_HOME);
    battery_tick(39, 64010000);
    demo_duel_exit();
}

int main(void)
{
    test_delayed_first_read();
    test_timing_and_celebration_unaffected();
    test_idle_exit_and_reentry();
    puts("Time Challenge demo: delayed battery, recovery, timing, celebration, idle, exit/reentry PASS");
    return 0;
}
