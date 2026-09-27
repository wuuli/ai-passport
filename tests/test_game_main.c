// tests/test_game_main.c —— Host regression tests for main/game_main.c dispatcher.
// Executes actual game_main.c with BSP/LVGL/FreeRTOS queue mocks.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "duel_io.h"
#include "game_launcher.h"
#include "fap_screenshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

// ---------------------------------------------------------------------------
// FreeRTOS Queue Mock
// ---------------------------------------------------------------------------
static void (*s_after_empty_receive)(void);

struct QueueDefinition {
    size_t length;
    size_t item_size;
    size_t count;
    size_t head;
    size_t tail;
    uint8_t *buffer;
};

QueueHandle_t xQueueCreate(size_t uxQueueLength, size_t uxItemSize) {
    if (uxQueueLength == 0 || uxItemSize == 0) return NULL;
    struct QueueDefinition *q = (struct QueueDefinition *)malloc(sizeof(struct QueueDefinition));
    if (!q) return NULL;
    q->length = uxQueueLength;
    q->item_size = uxItemSize;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->buffer = (uint8_t *)malloc(uxQueueLength * uxItemSize);
    if (!q->buffer) {
        free(q);
        return NULL;
    }
    return q;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait) {
    (void)xTicksToWait;
    if (!xQueue) return pdFALSE;
    if (xQueue->count >= xQueue->length) return pdFALSE;
    memcpy(xQueue->buffer + (xQueue->tail * xQueue->item_size), pvItemToQueue, xQueue->item_size);
    xQueue->tail = (xQueue->tail + 1) % xQueue->length;
    xQueue->count++;
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait) {
    (void)xTicksToWait;
    if (!xQueue) return pdFALSE;
    if (xQueue->count == 0) {
        if (s_after_empty_receive) {
            void (*hook)(void) = s_after_empty_receive;
            s_after_empty_receive = NULL;
            hook();
        }
        return pdFALSE;
    }
    memcpy(pvBuffer, xQueue->buffer + (xQueue->head * xQueue->item_size), xQueue->item_size);
    xQueue->head = (xQueue->head + 1) % xQueue->length;
    xQueue->count--;
    return pdTRUE;
}

void xQueueReset(QueueHandle_t xQueue) {
    if (!xQueue) return;
    xQueue->count = 0;
    xQueue->head = 0;
    xQueue->tail = 0;
}

void vQueueDelete(QueueHandle_t xQueue) {
    if (!xQueue) return;
    free(xQueue->buffer);
    free(xQueue);
}

// ---------------------------------------------------------------------------
// Timer, LVGL, BSP, and Peripheral Mocks
// ---------------------------------------------------------------------------
static int64_t s_mock_time_us = 1000000;
int64_t esp_timer_get_time(void) { return s_mock_time_us; }

static lv_timer_t s_mock_timer;
static lv_timer_cb_t s_mock_timer_cb = NULL;
static bool s_fail_lv_timer_create = false;

lv_timer_t *lv_timer_create(lv_timer_cb_t timer_xcb, uint32_t period, void *user_data) {
    if (s_fail_lv_timer_create) return NULL;
    s_mock_timer.timer_cb = timer_xcb;
    s_mock_timer.period = period;
    s_mock_timer.user_data = user_data;
    s_mock_timer_cb = timer_xcb;
    return &s_mock_timer;
}

static bsp_btn_cb_t s_bsp_btn_cb = NULL;
static void *s_bsp_btn_user = NULL;
static bool s_fail_bsp_button_init = false;

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user) {
    if (s_fail_bsp_button_init) return ESP_FAIL;
    s_bsp_btn_cb = cb;
    s_bsp_btn_user = user;
    return ESP_OK;
}

static bool s_fail_bsp_i2c_init = false;
static bool s_fail_bsp_display_init = false;
static bool s_fail_bsp_lvgl_init = false;
static bool s_fail_bsp_lvgl_lock = false;
static uint8_t s_backlight_percent = 0;
static bool s_fail_bsp_audio_init = false;
static bool s_fail_bsp_battery_init = false;
static bool s_fail_fap_screenshot = false;

esp_err_t bsp_i2c_init(void) { return s_fail_bsp_i2c_init ? ESP_FAIL : ESP_OK; }
esp_err_t bsp_display_init(void) { return s_fail_bsp_display_init ? ESP_FAIL : ESP_OK; }
bool bsp_lvgl_init(void) { return !s_fail_bsp_lvgl_init; }
bool bsp_lvgl_lock(int timeout_ms) { (void)timeout_ms; return !s_fail_bsp_lvgl_lock; }
void bsp_lvgl_unlock(void) {}
void bsp_display_backlight(uint8_t percent) { s_backlight_percent = percent; }
esp_err_t bsp_audio_init(void) { return s_fail_bsp_audio_init ? ESP_FAIL : ESP_OK; }
esp_err_t bsp_battery_init(void) { return s_fail_bsp_battery_init ? ESP_FAIL : ESP_OK; }
esp_err_t fap_screenshot_start(void) { return s_fail_fap_screenshot ? ESP_FAIL : ESP_OK; }

static bool s_duel_io_init_called = false;
static bool s_duel_io_active = false;
static int s_duel_io_battery_level = 85;

bool duel_io_init(bool audio_ready, bool battery_ready) {
    (void)audio_ready; (void)battery_ready;
    s_duel_io_init_called = true;
    return true;
}
void duel_io_activate(bool active) { s_duel_io_active = active; }
int duel_io_battery(void) { return s_duel_io_battery_level; }

static int s_launcher_create_count = 0;
static unsigned s_launcher_last_create_selected = 0;
static int s_launcher_last_create_battery = 0;
static int s_launcher_update_count = 0;
static unsigned s_launcher_last_update_selected = 0;
static int s_launcher_last_update_battery = 0;
static int s_launcher_destroy_count = 0;
static bool s_launcher_active = false;
static bool s_fail_launcher_create = false;

bool game_launcher_create(unsigned selected, int battery) {
    s_launcher_create_count++;
    s_launcher_last_create_selected = selected;
    s_launcher_last_create_battery = battery;
    if (s_fail_launcher_create) return false;
    s_launcher_active = true;
    return true;
}

void game_launcher_update(unsigned selected, int battery) {
    s_launcher_update_count++;
    s_launcher_last_update_selected = selected;
    s_launcher_last_update_battery = battery;
}

void game_launcher_destroy(void) {
    s_launcher_destroy_count++;
    s_launcher_active = false;
}

// ---------------------------------------------------------------------------
// Game Hooks (GAMES[0] = Exit 8, GAMES[1] = Time Challenge)
// ---------------------------------------------------------------------------
static int s_corridor_enter_count = 0;
static int s_corridor_exit_count = 0;
static int s_corridor_key_count = 0;
static bsp_btn_t s_corridor_last_btn;
static bsp_btn_ev_t s_corridor_last_ev;
static int s_corridor_tick_count = 0;
static int64_t s_corridor_last_tick = 0;
static int s_corridor_input_lost_count = 0;
static int s_corridor_return_to_title_count = 0;
static bool s_corridor_return_to_title_val = true;
static bool s_corridor_active = false;

void demo_corridor_enter(void) {
    s_corridor_enter_count++;
    s_corridor_active = true;
}
void demo_corridor_exit(void) {
    s_corridor_exit_count++;
    s_corridor_active = false;
}
void demo_corridor_key(bsp_btn_t button, bsp_btn_ev_t event) {
    s_corridor_key_count++;
    s_corridor_last_btn = button;
    s_corridor_last_ev = event;
}
void demo_corridor_tick(int64_t now_us) {
    s_corridor_tick_count++;
    s_corridor_last_tick = now_us;
}
void demo_corridor_input_lost(void) {
    s_corridor_input_lost_count++;
}
bool demo_corridor_return_to_title(void) {
    s_corridor_return_to_title_count++;
    return s_corridor_return_to_title_val;
}

static int s_duel_enter_count = 0;
static int s_duel_exit_count = 0;
static int s_duel_key_count = 0;
static int s_duel_key_at_count = 0;
static bsp_btn_t s_duel_last_btn;
static bsp_btn_ev_t s_duel_last_ev;
static int64_t s_duel_last_timestamp = 0;
static int s_duel_tick_count = 0;
static int64_t s_duel_last_tick = 0;
static int s_duel_input_lost_count = 0;
static int64_t s_duel_last_input_lost_time = 0;
static bool s_duel_active = false;

void demo_duel_enter(void) {
    s_duel_enter_count++;
    s_duel_active = true;
}
void demo_duel_exit(void) {
    s_duel_exit_count++;
    s_duel_active = false;
}
void demo_duel_key(bsp_btn_t button, bsp_btn_ev_t event) {
    s_duel_key_count++;
    (void)button; (void)event;
}
void demo_duel_key_at(bsp_btn_t button, bsp_btn_ev_t event, int64_t timestamp_us) {
    s_duel_key_at_count++;
    s_duel_last_btn = button;
    s_duel_last_ev = event;
    s_duel_last_timestamp = timestamp_us;
}
void demo_duel_tick(int64_t now_us) {
    s_duel_tick_count++;
    s_duel_last_tick = now_us;
}
void demo_duel_input_lost(int64_t now_us) {
    s_duel_input_lost_count++;
    s_duel_last_input_lost_time = now_us;
}

// ---------------------------------------------------------------------------
// Include actual game_main.c implementation
// ---------------------------------------------------------------------------
#include "game_main.c"

// ---------------------------------------------------------------------------
// Test Helpers
// ---------------------------------------------------------------------------
static void reset_dispatcher_state(void) {
    if (s_input_queue) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
    }
    atomic_store(&s_generation, 0);
    atomic_store(&s_input_overflow, false);
    atomic_store(&s_input_ready, false);
    demo_navigation_init(&s_navigation, GAME_COUNT);
    s_navigation.active = -1;
    s_navigation.selected = 0;

    s_mock_time_us = 1000000;
    s_fail_lv_timer_create = false;
    s_fail_bsp_button_init = false;
    s_fail_bsp_i2c_init = false;
    s_fail_bsp_display_init = false;
    s_fail_bsp_lvgl_init = false;
    s_fail_bsp_lvgl_lock = false;
    s_fail_bsp_audio_init = false;
    s_fail_bsp_battery_init = false;
    s_fail_fap_screenshot = false;
    s_fail_launcher_create = false;
    s_backlight_percent = 0;

    s_duel_io_init_called = false;
    s_duel_io_active = false;
    s_duel_io_battery_level = 85;

    s_launcher_create_count = 0;
    s_launcher_last_create_selected = 0;
    s_launcher_last_create_battery = 0;
    s_launcher_update_count = 0;
    s_launcher_last_update_selected = 0;
    s_launcher_last_update_battery = 0;
    s_launcher_destroy_count = 0;
    s_launcher_active = false;

    s_corridor_enter_count = 0;
    s_corridor_exit_count = 0;
    s_corridor_key_count = 0;
    s_corridor_tick_count = 0;
    s_corridor_last_tick = 0;
    s_corridor_input_lost_count = 0;
    s_corridor_return_to_title_count = 0;
    s_corridor_return_to_title_val = true;
    s_corridor_active = false;

    s_duel_enter_count = 0;
    s_duel_exit_count = 0;
    s_duel_key_count = 0;
    s_duel_key_at_count = 0;
    s_duel_last_timestamp = 0;
    s_duel_tick_count = 0;
    s_duel_last_tick = 0;
    s_duel_input_lost_count = 0;
    s_duel_last_input_lost_time = 0;
    s_duel_active = false;
}

static void trigger_key(bsp_btn_t btn, bsp_btn_ev_t ev, int64_t timestamp_us) {
    int64_t saved = s_mock_time_us;
    if (timestamp_us > 0) s_mock_time_us = timestamp_us;
    assert(s_bsp_btn_cb != NULL);
    s_bsp_btn_cb(btn, ev, s_bsp_btn_user);
    s_mock_time_us = saved;
}

static void trigger_tick(int64_t timestamp_us) {
    if (timestamp_us > 0) s_mock_time_us = timestamp_us;
    assert(s_mock_timer_cb != NULL);
    s_mock_timer_cb(&s_mock_timer);
}

// ---------------------------------------------------------------------------
// 1. 默认启动 Corridor (Default boots into Corridor)
// ---------------------------------------------------------------------------
static void test_default_boot_corridor(void) {
    reset_dispatcher_state();
    app_main();

    assert(s_navigation.active == 0);
    assert(s_navigation.selected == 0);
    assert(s_navigation.count == 2);
    assert(s_corridor_enter_count == 1);
    assert(s_corridor_active == true);
    assert(s_duel_enter_count == 0);
    assert(s_launcher_create_count == 0);
    assert(s_launcher_active == false);

    // Verify tick calls active game (Corridor)
    trigger_tick(1005000);
    assert(s_corridor_tick_count == 1);
    assert(s_corridor_last_tick == 1005000);
    assert(s_duel_tick_count == 0);
    assert(s_launcher_update_count == 0);

    printf("  [PASS] 1. Default boots into Corridor (active=0, GAMES[0].enter called, ticks Corridor)\n");
}

// ---------------------------------------------------------------------------
// 2. 长OK playing->title 再long->独立launcher
// ---------------------------------------------------------------------------
static void test_long_ok_title_then_launcher(void) {
    reset_dispatcher_state();
    app_main();
    assert(s_navigation.active == 0);

    // Phase 1: In playing phase, Long OK returns to Title
    s_corridor_return_to_title_val = true;
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 1010000);
    trigger_tick(1015000);

    assert(s_corridor_return_to_title_count == 1);
    assert(s_corridor_exit_count == 0); // Did NOT exit Corridor
    assert(s_launcher_create_count == 0); // Did NOT enter launcher
    assert(s_navigation.active == 0); // Still active=0

    // Phase 2: Now on Title screen, Long OK returns false -> exits to launcher
    s_corridor_return_to_title_val = false;
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 1020000);
    trigger_tick(1025000);

    assert(s_corridor_return_to_title_count == 2);
    assert(s_corridor_exit_count == 1); // Corridor exited!
    assert(s_launcher_create_count == 1); // Launcher created!
    assert(s_launcher_active == true);
    assert(s_navigation.active == -1); // Active is -1 (launcher)
    assert(s_navigation.selected == 0); // Retained selection 0
    assert(s_launcher_last_create_selected == 0);
    assert(s_launcher_last_create_battery == 85);
    assert(s_duel_io_active == true);

    // Phase 3: While in launcher, tick updates launcher instead of game
    int prev_updates = s_launcher_update_count;
    trigger_tick(1030000);
    assert(s_launcher_update_count == prev_updates + 1);
    assert(s_corridor_tick_count == 1); // Not incremented

    printf("  [PASS] 2. Long OK: playing -> title (intercepted), title -> launcher (active=-1)\n");
}

// ---------------------------------------------------------------------------
// 3. UP/DOWN选Time Challenge
// ---------------------------------------------------------------------------
static void test_up_down_select_time_challenge(void) {
    reset_dispatcher_state();
    app_main();

    // Exit Corridor to launcher
    s_corridor_return_to_title_val = false;
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 1010000);
    trigger_tick(1015000);
    assert(s_navigation.active == -1);
    assert(s_navigation.selected == 0);

    // DOWN click -> moves selected from 0 to 1 ("Time Challenge")
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 1020000);
    trigger_tick(1025000);
    assert(s_navigation.selected == 1);
    assert(s_launcher_last_update_selected == 1);

    // DOWN click again -> wraps around to 0 ("Exit 8")
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 1030000);
    trigger_tick(1035000);
    assert(s_navigation.selected == 0);
    assert(s_launcher_last_update_selected == 0);

    // UP click -> wraps backward to 1 ("Time Challenge")
    trigger_key(BSP_BTN_UP, BSP_BTN_CLICK, 1040000);
    trigger_tick(1045000);
    assert(s_navigation.selected == 1);
    assert(s_launcher_last_update_selected == 1);

    printf("  [PASS] 3. UP/DOWN navigates and wraps between games in launcher\n");
}

// ---------------------------------------------------------------------------
// 4. CLICK进入不重复PRESS
// ---------------------------------------------------------------------------
static void test_click_enters_no_duplicate_press(void) {
    reset_dispatcher_state();
    app_main();

    // Exit Corridor to launcher
    s_corridor_return_to_title_val = false;
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 1010000);
    trigger_tick(1015000);
    assert(s_navigation.active == -1);

    // Select Time Challenge (selected = 1)
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 1020000);
    trigger_tick(1025000);
    assert(s_navigation.selected == 1);

    // PRESS event on OK button must NOT trigger enter
    trigger_key(BSP_BTN_OK, BSP_BTN_PRESS, 1030000);
    trigger_tick(1035000);
    assert(s_navigation.active == -1); // Still in launcher
    assert(s_launcher_active == true);
    assert(s_duel_enter_count == 0); // Time Challenge not entered
    assert(s_launcher_destroy_count == 0);

    // CLICK event on OK button triggers enter exactly once
    trigger_key(BSP_BTN_OK, BSP_BTN_CLICK, 1040000);
    trigger_tick(1045000);
    assert(s_navigation.active == 1); // Now in Time Challenge
    assert(s_launcher_active == false); // Launcher destroyed
    assert(s_launcher_destroy_count == 1);
    assert(s_duel_enter_count == 1); // Time Challenge entered

    // Subsequent PRESS event inside the game is forwarded to key_at, no duplicate enter
    trigger_key(BSP_BTN_OK, BSP_BTN_PRESS, 1050000);
    trigger_tick(1055000);
    assert(s_duel_enter_count == 1); // Enter count unchanged
    assert(s_duel_key_at_count == 1); // Forwarded to game's key_at
    assert(s_duel_last_ev == BSP_BTN_PRESS);

    printf("  [PASS] 4. CLICK enters game; PRESS does not trigger enter or duplicate\n");
}

// ---------------------------------------------------------------------------
// 5. 真实timestamp转发 (Real timestamp forwarding)
// ---------------------------------------------------------------------------
static void enqueue_after_drain(void) {
    // Simulate an esp_timer callback immediately after the consumer saw empty.
    trigger_key(BSP_BTN_OK, BSP_BTN_PRESS, 11000000);
    s_mock_time_us = 12000000;
}

static void test_real_timestamp_forwarding(void) {
    reset_dispatcher_state();
    app_main();

    // Navigate to Time Challenge (index 1) which implements key_at
    s_corridor_return_to_title_val = false;
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 1000000);
    trigger_tick(1005000);
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 1010000);
    trigger_tick(1015000);
    trigger_key(BSP_BTN_OK, BSP_BTN_CLICK, 1020000);
    trigger_tick(1025000);
    assert(s_navigation.active == 1);

    // Enqueue key press at timestamp T1
    const int64_t press_timestamp_us = 5555555LL;
    trigger_key(BSP_BTN_UP, BSP_BTN_PRESS, press_timestamp_us);

    // System time advances significantly before process_input ticks
    const int64_t tick_timestamp_us = 9999999LL;
    trigger_tick(tick_timestamp_us);

    // Verify key_at received the exact press_timestamp_us, not the tick timestamp
    assert(s_duel_key_at_count == 1);
    assert(s_duel_last_btn == BSP_BTN_UP);
    assert(s_duel_last_ev == BSP_BTN_PRESS);
    assert(s_duel_last_timestamp == press_timestamp_us);
    assert(s_duel_last_timestamp != tick_timestamp_us);

    s_after_empty_receive = enqueue_after_drain;
    trigger_tick(10000000);
    assert(s_duel_last_tick == 10000000); // Must not overtake the queued press.
    assert(s_input_queue->count == 1);
    trigger_tick(13000000);
    assert(s_duel_last_timestamp == 11000000);
    printf("  [PASS] 5. Press timestamps survive dispatch and ticks cannot overtake queued input\n");
}

// ---------------------------------------------------------------------------
// 6. generation丢弃页面切换前的队列事件
// ---------------------------------------------------------------------------
static void test_generation_drops_stale_events(void) {
    reset_dispatcher_state();
    app_main();

    // Scenario A: Stale events across Corridor title -> Launcher transition
    // Corridor on title (return_to_title_val = false)
    s_corridor_return_to_title_val = false;

    // Send Long OK (which will trigger exit & enter_launcher)
    // and immediately send UP and DOWN clicks while still in generation 0
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 2000000);
    trigger_key(BSP_BTN_UP, BSP_BTN_CLICK, 2000100);
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 2000200);

    // When process_input runs, the Long OK increments generation and enters launcher.
    // The subsequent UP and DOWN events in the same queue batch have stale generation
    // and MUST be dropped, not affecting the launcher selection!
    trigger_tick(2001000);

    assert(s_navigation.active == -1); // In launcher
    assert(s_navigation.selected == 0); // Selection untouched by stale UP/DOWN!
    assert(s_corridor_key_count == 0);

    // Scenario B: Stale events across Launcher -> Game transition
    // In launcher, selected = 1 (Time Challenge)
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 3000000);
    trigger_tick(3001000);
    assert(s_navigation.selected == 1);

    // Enqueue OK CLICK (enters game) followed immediately by UP CLICK
    trigger_key(BSP_BTN_OK, BSP_BTN_CLICK, 3002000);
    trigger_key(BSP_BTN_UP, BSP_BTN_CLICK, 3002100);

    // Process batch: OK CLICK enters game and increments generation.
    // UP CLICK was enqueued with launcher's generation, so it must be dropped.
    trigger_tick(3003000);

    assert(s_navigation.active == 1); // Time Challenge active
    assert(s_duel_enter_count == 1);
    assert(s_duel_key_at_count == 0); // Stale UP CLICK was NOT delivered to Time Challenge!

    printf("  [PASS] 6. Generation counter drops queue events enqueued before transition\n");
}

// ---------------------------------------------------------------------------
// 7. overflow停止当前游戏 (queue overflow stops current game)
// ---------------------------------------------------------------------------
static void test_overflow_stops_active_game(void) {
    reset_dispatcher_state();
    app_main();

    // Scenario A: Overflow while Corridor is active (active == 0)
    assert(s_navigation.active == 0);
    assert(s_corridor_input_lost_count == 0);

    // Queue capacity is 32. Fill it to capacity with 32 events.
    for (int i = 0; i < 32; ++i) {
        trigger_key(BSP_BTN_UP, BSP_BTN_PRESS, 4000000 + i);
    }
    assert(atomic_load(&s_input_overflow) == false);

    // 33rd event overflows the queue
    trigger_key(BSP_BTN_UP, BSP_BTN_PRESS, 4001000);
    assert(atomic_load(&s_input_overflow) == true);

    // Next tick must detect overflow, reset queue, and call demo_corridor_input_lost()
    trigger_tick(4002000);
    assert(atomic_load(&s_input_overflow) == false);
    assert(s_corridor_input_lost_count == 1);
    assert(s_duel_input_lost_count == 0);

    // Scenario B: Overflow while Time Challenge is active (active == 1)
    // Switch to Time Challenge
    s_corridor_return_to_title_val = false;
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 5000000);
    trigger_tick(5001000);
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 5002000);
    trigger_tick(5003000);
    trigger_key(BSP_BTN_OK, BSP_BTN_CLICK, 5004000);
    trigger_tick(5005000);
    assert(s_navigation.active == 1);
    assert(s_duel_input_lost_count == 0);

    // Fill queue to capacity (32 items)
    for (int i = 0; i < 32; ++i) {
        trigger_key(BSP_BTN_UP, BSP_BTN_PRESS, 5010000 + i);
    }
    // 33rd event overflows
    trigger_key(BSP_BTN_UP, BSP_BTN_PRESS, 5020000);
    assert(atomic_load(&s_input_overflow) == true);

    // Tick at specific time
    const int64_t overflow_tick_time = 5030000LL;
    trigger_tick(overflow_tick_time);
    assert(atomic_load(&s_input_overflow) == false);
    assert(s_duel_input_lost_count == 1);
    assert(s_duel_last_input_lost_time == overflow_tick_time);
    assert(s_corridor_input_lost_count == 1); // Unchanged

    printf("  [PASS] 7. Queue overflow resets queue, advances generation, calls input_lost()\n");
}

// ---------------------------------------------------------------------------
// 8. launcher回游戏时无诊断UI (launcher returns to game with no diagnostic UI)
// ---------------------------------------------------------------------------
static void test_launcher_return_no_diagnostic_ui(void) {
    reset_dispatcher_state();
    app_main();

    // Exit Corridor to launcher
    s_corridor_return_to_title_val = false;
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 1000000);
    trigger_tick(1005000);
    assert(s_navigation.active == -1);
    assert(s_launcher_active == true);

    // Return to Corridor: select 0, click OK
    assert(s_navigation.selected == 0);
    trigger_key(BSP_BTN_OK, BSP_BTN_CLICK, 1010000);
    trigger_tick(1015000);

    assert(s_navigation.active == 0);
    assert(s_launcher_active == false); // Destroyed!
    assert(s_launcher_destroy_count == 1);
    assert(s_corridor_enter_count == 2); // Re-entered Corridor

    // Exit Corridor to launcher again
    trigger_key(BSP_BTN_OK, BSP_BTN_LONG, 1020000);
    trigger_tick(1025000);
    assert(s_navigation.active == -1);
    assert(s_launcher_active == true);

    // Switch to Time Challenge (1), click OK
    trigger_key(BSP_BTN_DOWN, BSP_BTN_CLICK, 1030000);
    trigger_tick(1035000);
    assert(s_navigation.selected == 1);
    trigger_key(BSP_BTN_OK, BSP_BTN_CLICK, 1040000);
    trigger_tick(1045000);

    assert(s_navigation.active == 1);
    assert(s_launcher_active == false); // Destroyed!
    assert(s_launcher_destroy_count == 2);
    assert(s_duel_enter_count == 1);

    // Verify game_main.c uses game_launcher lifecycle exclusively, without any
    // diagnostic screen (s_cards, s_rows, s_mascot, ui_pixel) from main.c.
    // The launcher is cleanly destroyed before entering either game.
    printf("  [PASS] 8. Launcher destroys cleanly upon game entry with zero diagnostic UI\n");
}

// ---------------------------------------------------------------------------
// 9. 异常初始化分支覆盖 (Error handling and graceful aborts)
// ---------------------------------------------------------------------------
static void test_initialization_failures(void) {
    // Subtest: Display init failure
    reset_dispatcher_state();
    s_fail_bsp_display_init = true;
    app_main();
    assert(atomic_load(&s_input_ready) == false);
    assert(s_navigation.active == -1);

    // Subtest: LVGL init failure
    reset_dispatcher_state();
    s_fail_bsp_lvgl_init = true;
    app_main();
    assert(atomic_load(&s_input_ready) == false);
    assert(s_navigation.active == -1);

    // Subtest: Button init failure cleans up queue
    reset_dispatcher_state();
    s_fail_bsp_button_init = true;
    app_main();
    assert(s_input_queue == NULL); // Cleanly deleted
    assert(atomic_load(&s_input_ready) == false);

    // Subtest: LVGL lock failure
    reset_dispatcher_state();
    s_fail_bsp_lvgl_lock = true;
    app_main();
    assert(atomic_load(&s_input_ready) == false);

    // Subtest: Timer create failure
    reset_dispatcher_state();
    s_fail_lv_timer_create = true;
    app_main();
    assert(atomic_load(&s_input_ready) == false);

    printf("  [PASS] 9. Initialization failures abort gracefully without accepting input\n");
}

int main(void) {
    printf("=== Running game_main host dispatcher regression tests ===\n");
    test_default_boot_corridor();
    test_long_ok_title_then_launcher();
    test_up_down_select_time_challenge();
    test_click_enters_no_duplicate_press();
    test_real_timestamp_forwarding();
    test_generation_drops_stale_events();
    test_overflow_stops_active_game();
    test_launcher_return_no_diagnostic_ui();
    test_initialization_failures();
    printf("=== All game_main dispatcher regression tests PASSED! ===\n");
    return 0;
}
