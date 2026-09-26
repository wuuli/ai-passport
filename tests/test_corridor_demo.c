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
static bool s_fail_renderer = false;
ec_renderer_t *ec_renderer_create(const uint8_t *sprite, size_t sprite_size) {
    (void)sprite; (void)sprite_size;
    if (s_fail_renderer) return NULL;
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
const lv_font_t corridor_title_font = {0};

#include "demo_corridor.c"

static void setup_exit_7(bool anomaly) {
    demo_corridor_enter();
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    s_now_us += 60000;
    demo_corridor_tick(s_now_us);
    assert(!dirty);
    assert(game.phase == EC_PLAYING);
    assert(lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));

    game.score = game.hud_score = 7;
    game.anomaly = anomaly ? EC_RED_LIGHTS : EC_NORMAL;
    game.entry_exit = false;
    game.x = 4.79f;
    game.z = -26.8f;
    game.yaw = PI / 2.0f;
    game.walking = true;
    dirty = false;
}

static void test_title_screen(void) {
    demo_corridor_enter();
    assert(game.phase == EC_TITLE);
    assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(subtitle->text, "地下通道") == 0);
    assert(strcmp(title->text, "8号出口") == 0);
    assert(strcmp(caption->text, "顶部键：左转\n中部键：右转\n底部 OK 键：行走／停步\n拐角自动转向，停后按 OK") == 0);
    assert(strcmp(prompt->text, "按 OK 进入") == 0);
    assert(title->font == &corridor_title_font);
    assert(overlay->y == 0 && overlay->h == 320);
    assert(subtitle->y == 66 && title->y == 94 && caption->y == 160 && prompt->y == 252 && footer->y == 287);
    assert(strcmp(footer->text, "长按 OK 返回") == 0);
    assert(rule->w == 32 && rule->h == 2 && rule->x == 104 && rule->y == 145);
    assert(rule->bg_color == 0xd6b954 && rule->bg_opa == LV_OPA_60);
    assert(overlay->bg_color == 0x080d0d && overlay->bg_opa == LV_OPA_70);
    assert(bar->bg_opa == LV_OPA_TRANSP && strcmp(status->text, "") == 0);
    assert(strcmp(battery->text, "85%") == 0 && battery->text_color == 0xefeee8);
    s_battery = -1; hud();
    assert(strcmp(battery->text, "--") == 0);
    s_battery = 85; hud();

    /* Test prompt fade progression: 153 (LV_OPA_60) -> 204 (LV_OPA_80 at 400ms) -> 255 (LV_OPA_COVER at 800ms) */
    assert(prompt->text_opa == LV_OPA_60);
    s_now_us += 400000;
    demo_corridor_tick(s_now_us);
    assert(prompt->text_opa == LV_OPA_80);
    s_now_us += 400000;
    demo_corridor_tick(s_now_us);
    assert(prompt->text_opa == LV_OPA_COVER);
    s_now_us += 800000;
    demo_corridor_tick(s_now_us);
    assert(prompt->text_opa == LV_OPA_COVER);

    /* UP/DOWN and OK PRESS do not change phase during title */
    demo_corridor_key(BSP_BTN_UP, BSP_BTN_PRESS);
    assert(game.phase == EC_TITLE);
    demo_corridor_key(BSP_BTN_DOWN, BSP_BTN_PRESS);
    assert(game.phase == EC_TITLE);
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_PRESS);
    assert(game.phase == EC_TITLE);

    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(game.phase == EC_PLAYING);
    assert(lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    assert(bar->bg_opa == LV_OPA_80);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    printf("test_title_screen PASS\n");
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
    assert(strcmp(title->text, "8号出口") == 0);
    assert(strcmp(caption->text, "你已走出通道") == 0);
    assert(overlay->bg_color == 0xf3f4f0 && overlay->bg_opa == LV_OPA_90);
    assert(title->text_color == 0x242b2b && title->font == &corridor_title_font);
    assert(bar->bg_opa == LV_OPA_TRANSP);
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
    assert(strcmp(title->text, "8号出口") == 0);
    assert(strcmp(caption->text, "你已走出通道") == 0);
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
    assert(strcmp(title->text, "8号出口") == 0);
    assert(strcmp(caption->text, "你已走出通道") == 0);

    /* Advance several ticks to verify cleared screen holds, score stays 8,
     * and no automatic restart occurs before OK is pressed. */
    for (int i = 0; i < 5; ++i) {
        s_now_us += 60000;
        demo_corridor_tick(s_now_us);
        assert(game.phase == EC_CLEARED);
        assert(game.score == 8);
        assert(!game.walking);
        assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
        assert(strcmp(title->text, "8号出口") == 0);
        assert(strcmp(caption->text, "你已走出通道") == 0);
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
    assert(strstr(status->text, "出口 7") != NULL);
    assert(game.hud_score_pending);
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
    assert(strstr(status->text, "出口 7") != NULL);
    assert(game.hud_score_pending);
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

static void test_reentry_lifecycle(void) {
    printf("--- Running test_reentry_lifecycle ---\n");
    demo_corridor_enter();
    assert(game.phase == EC_TITLE);
    assert(presentation_phase == EC_TITLE);
    assert(prompt->text_opa == LV_OPA_60);
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(game.phase == EC_PLAYING);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    assert(renderer == NULL && screen == NULL && overlay == NULL);

    /* Re-entering should cleanly re-initialize state and prompt fade */
    demo_corridor_enter();
    assert(game.phase == EC_TITLE);
    assert(presentation_phase == EC_TITLE);
    assert(prompt->text_opa == LV_OPA_60);
    assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    printf("test_reentry_lifecycle PASS\n");
}

static void test_return_to_title(void) {
    demo_corridor_enter();
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(game.phase == EC_PLAYING && game.walking);
    assert(demo_corridor_return_to_title());
    assert(game.phase == EC_TITLE && game.score == 0 && !game.walking);
    assert(!lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(prompt->text, "按 OK 进入") == 0);
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(game.phase == EC_PLAYING && !game.walking);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
}

static void test_oom_fallback(void) {
    printf("--- Running test_oom_fallback ---\n");
    s_fail_renderer = true;
    demo_corridor_enter();
    assert(renderer == NULL);
    assert(pixels == NULL);
    assert(title->font == &corridor_font);
    assert(strcmp(title->text, "内存不足") == 0);
    assert(strcmp(caption->text, "长按 OK 返回") == 0);
    assert(strcmp(status->text, "长按 OK 返回") == 0);
    assert(lv_obj_has_flag(prompt, LV_OBJ_FLAG_HIDDEN));
    assert(!demo_corridor_return_to_title());

    /* Tick and keys do not crash when renderer is NULL */
    s_now_us += 60000;
    demo_corridor_tick(s_now_us);
    demo_corridor_key(BSP_BTN_OK, BSP_BTN_CLICK);
    demo_corridor_exit();
    assert(lv_obj_get_alive_count() == 0);
    s_fail_renderer = false;
    printf("test_oom_fallback PASS\n");
}

int main(void) {
    test_title_screen();
    test_reentry_lifecycle();
    test_return_to_title();
    test_oom_fallback();
    test_eighth_choice_enters_exit_walk();
    test_clear_with_dirty_false();
    test_clear_throttled_next_frame();
    test_ok_restarts_game();
    test_wrong_exit_remains_playing();
    puts("All corridor demo tests PASS");
    return 0;
}
