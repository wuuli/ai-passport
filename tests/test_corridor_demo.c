#define asm __asm__
#include "demo.h"
#include "corridor_game.h"
#include "corridor_render.h"
#include "duel_io.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "lvgl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846f

static int64_t s_now_us = 1000000;
int64_t esp_timer_get_time(void) { return s_now_us; }
uint32_t esp_random(void) { return 42; }

static int s_battery = 85;
int duel_io_battery(void) { return s_battery; }
void duel_io_activate(bool active) { (void)active; }
void duel_io_sound(bool enabled, bool music, duel_cue_t cue) {
    (void)enabled; (void)music; (void)cue;
}

struct ec_renderer { int dummy; };
static struct ec_renderer s_dummy_renderer;
ec_renderer_t *ec_renderer_create(const uint8_t *sprite, size_t sprite_size) {
    (void)sprite; (void)sprite_size;
    return &s_dummy_renderer;
}
void ec_renderer_destroy(ec_renderer_t *r) { (void)r; }
void ec_renderer_draw(ec_renderer_t *r, const ec_game_t *g, uint8_t *img) {
    (void)r; (void)g; (void)img;
}
size_t ec_renderer_work_bytes(void) { return 0; }
void ec_renderer_set_clock(ec_renderer_t *r, uint32_t (*c)(void)) { (void)r; (void)c; }
void ec_renderer_profile(const ec_renderer_t *r, uint32_t stages[3]) {
    (void)r; stages[0] = stages[1] = stages[2] = 0;
}

const uint8_t ec_sprite_start[1] __asm__("_binary_commuter_device_bin_start") = {0};
const uint8_t ec_sprite_end[1] __asm__("_binary_commuter_device_bin_end") = {0};
const lv_font_t corridor_font = {0};

#include "demo_corridor.c"

static void setup_exit_7(bool anomaly) {
    demo_corridor_enter();
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    s_now_us += 60000;
    demo_corridor_tick(s_now_us);
    assert(!dirty);
    assert(game.phase == EC_PLAYING);
    assert(lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));

    game.score = 7;
    game.anomaly = anomaly ? EC_RED_LIGHTS : EC_NORMAL;
    game.entry_exit = false;
    game.x = 4.79f;
    game.z = -26.8f;
    game.yaw = PI / 2.0f;
    game.walking = true;
    dirty = false;
}

static void test_clear_with_dirty_false(void) {
    printf("--- Running test_clear_with_dirty_false ---\n");
    setup_exit_7(false);
    game.phase = EC_EXITING;
    game.score = 8;
    game.x = 0;
    game.z = -10.99f;
    game.yaw = game.camera_yaw = 0;
    assert(!dirty);

    /* Advance time past 50ms render throttle and tick */
    s_now_us += 60000;
    demo_corridor_tick(s_now_us);

    /* Verification */
    assert(game.phase == EC_CLEARED);
    assert(game.score == 8);
    if (lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN)) {
        fprintf(stderr, "REGRESSION BUG DETECTED: game cleared (score=8, phase=EC_CLEARED), but overlay remains HIDDEN!\n");
        fprintf(stderr, "dirty=%d, title='%s', caption='%s'\n", dirty, title ? title->text : "", caption ? caption->text : "");
        assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    }
    assert(strcmp(title->text, "8 号出口") == 0);
    assert(strcmp(caption->text, "你已走出通道。\n\n按 OK 再走一次") == 0);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    printf("test_clear_with_dirty_false PASS\n");
}

static void test_clear_throttled_next_frame(void) {
    printf("--- Running test_clear_throttled_next_frame ---\n");
    setup_exit_7(false);
    game.phase = EC_EXITING;
    game.score = 8;
    game.x = 0;
    game.z = -10.99f;
    game.yaw = game.camera_yaw = 0;
    assert(!dirty);

    /* Tick inside 50ms throttle window (e.g. 20ms) */
    s_now_us += 20000;
    demo_corridor_tick(s_now_us);
    assert(game.phase == EC_CLEARED);

    /* Next display frame arrives after throttle window */
    s_now_us += 40000;
    demo_corridor_tick(s_now_us);

    if (lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN)) {
        fprintf(stderr, "REGRESSION BUG DETECTED: next display frame after throttled clear did not render cleared UI!\n");
        fprintf(stderr, "dirty=%d, title='%s', caption='%s'\n", dirty, title ? title->text : "", caption ? caption->text : "");
        assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    }
    assert(strcmp(title->text, "8 号出口") == 0);
    assert(strcmp(caption->text, "你已走出通道。\n\n按 OK 再走一次") == 0);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    printf("test_clear_throttled_next_frame PASS\n");
}

static void test_ok_restarts_game(void) {
    printf("--- Running test_ok_restarts_game ---\n");
    setup_exit_7(false);
    game.phase = EC_EXITING;
    game.score = 8;
    game.x = 0;
    game.z = -10.99f;
    game.yaw = game.camera_yaw = 0;
    s_now_us += 60000;
    demo_corridor_tick(s_now_us);
    assert(game.phase == EC_CLEARED);
    assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(title->text, "8 号出口") == 0);
    assert(strcmp(caption->text, "你已走出通道。\n\n按 OK 再走一次") == 0);

    /* Advance several ticks to verify cleared screen holds, score stays 8,
     * and no automatic restart occurs before OK is pressed. */
    for (int i = 0; i < 5; ++i) {
        s_now_us += 60000;
        demo_corridor_tick(s_now_us);
        assert(game.phase == EC_CLEARED);
        assert(game.score == 8);
        assert(!game.walking);
        assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
        assert(strcmp(title->text, "8 号出口") == 0);
        assert(strcmp(caption->text, "你已走出通道。\n\n按 OK 再走一次") == 0);
    }

    /* When cleared, pressing OK restarts game to exit 0 */
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(game.phase == EC_PLAYING);
    assert(game.score == 0);
    assert(lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strstr(status->text, "出口 0") != NULL);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    printf("test_ok_restarts_game PASS\n");
}

static void test_wrong_exit_remains_playing(void) {
    printf("--- Running test_wrong_exit_remains_playing ---\n");
    setup_exit_7(true); /* Anomaly present, but exiting forward -> wrong! */
    assert(!dirty);

    s_now_us += 60000;
    demo_corridor_tick(s_now_us);

    assert(game.score == 0);
    assert(game.phase == EC_PLAYING);
    assert(lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strstr(status->text, "出口 0") != NULL);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    printf("test_wrong_exit_remains_playing PASS\n");
}

static void test_eighth_choice_enters_exit_walk(void) {
    setup_exit_7(false);
    s_now_us += 60000;
    demo_corridor_tick(s_now_us);
    assert(game.phase == EC_EXITING && game.score == 8);
    assert(lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strstr(status->text, "出口 8") != NULL);
    unsigned frames_before = frame_count;
    s_now_us += 60000;
    demo_corridor_tick(s_now_us);
    assert(frame_count == frames_before + 1);
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(game.phase == EC_EXITING && game.score == 8 && !game.walking);
    demo_corridor_key(BSP_BTN_UP, BSP_BTN_PRESS);
    assert(game.phase == EC_EXITING && game.score == 8);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
}

int main(void) {
    test_eighth_choice_enters_exit_walk();
    test_clear_with_dirty_false();
    test_clear_throttled_next_frame();
    test_ok_restarts_game();
    test_wrong_exit_remains_playing();
    puts("All corridor demo tests PASS");
    return 0;
}
