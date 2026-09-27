// Independent game application. Hardware diagnostics retain their own main.c.
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "demo.h"
#include "demo_navigation.h"
#include "duel_io.h"
#include "game_launcher.h"
#include "fap_screenshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include <stdatomic.h>

static const char *TAG = "games";
static const demo_entry_t GAMES[] = {
    {.name = "Exit 8", .enter = demo_corridor_enter, .exit = demo_corridor_exit,
     .key = demo_corridor_key, .tick = demo_corridor_tick},
    {.name = "Time Challenge", .enter = demo_duel_enter, .exit = demo_duel_exit,
     .key_at = demo_duel_key_at, .tick = demo_duel_tick},
};
#define GAME_COUNT (sizeof(GAMES) / sizeof(GAMES[0]))

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
    int64_t timestamp_us;
    unsigned generation;
} game_input_t;

static demo_navigation_t s_navigation;
static QueueHandle_t s_input_queue;
static atomic_uint s_generation;
static atomic_bool s_input_overflow;
static atomic_bool s_input_ready;

// The shared worker only publishes atomic battery/audio state, never UI pointers.
// It lives for the application lifetime; page changes do not delete its resources.
static void enter_launcher(void)
{
    demo_navigation_complete_exit(&s_navigation);
    duel_io_activate(true);
    if (!game_launcher_create(s_navigation.selected, duel_io_battery())) {
        ESP_LOGE(TAG, "Unable to allocate game selection screen");
    }
}

static demo_nav_input_t navigation_input(bsp_btn_t btn, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK) return DEMO_NAV_INPUT_OTHER;
    if (btn == BSP_BTN_UP) return DEMO_NAV_INPUT_UP_CLICK;
    if (btn == BSP_BTN_DOWN) return DEMO_NAV_INPUT_DOWN_CLICK;
    if (btn == BSP_BTN_OK) return DEMO_NAV_INPUT_OK_CLICK;
    return DEMO_NAV_INPUT_OTHER;
}

// All game UI, including tick and teardown, is serialized by the LVGL timer.
// No background lifecycle service is started or stopped from these callbacks.
static void process_key(const game_input_t *input)
{
    const int active = s_navigation.active;
    if (active >= 0) {
        if (input->button == BSP_BTN_OK && input->event == BSP_BTN_LONG) {
            atomic_fetch_add(&s_generation, 1);
            if (active == 0 && demo_corridor_return_to_title()) return;
            GAMES[active].exit();
            enter_launcher();
        } else if (GAMES[active].key_at) {
            GAMES[active].key_at(input->button, input->event, input->timestamp_us);
        } else if (GAMES[active].key) {
            GAMES[active].key(input->button, input->event);
        }
        return;
    }
    demo_nav_result_t result = demo_navigation_handle(
        &s_navigation, navigation_input(input->button, input->event), true);
    if (result.action == DEMO_NAV_ACTION_REFRESH) {
        game_launcher_update(s_navigation.selected, duel_io_battery());
    } else if (result.action == DEMO_NAV_ACTION_ENTER) {
        atomic_fetch_add(&s_generation, 1);
        // Release launcher memory before allocating the raycaster frame buffer.
        game_launcher_destroy();
        GAMES[result.index].enter();
    }
}

// BSP callbacks run on the shared esp_timer task: capture the real press time
// and enqueue only. A dropped timing/steering input must stop the active game.
static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!atomic_load(&s_input_ready)) return;
    const game_input_t input = {button, event, esp_timer_get_time(),
                                atomic_load(&s_generation)};
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) {
        atomic_store(&s_input_overflow, true);
    }
}

static void process_input(lv_timer_t *timer)
{
    (void)timer;
    const int64_t now = esp_timer_get_time();
    if (atomic_exchange(&s_input_overflow, false)) {
        xQueueReset(s_input_queue);
        atomic_fetch_add(&s_generation, 1);
        if (s_navigation.active == 0) demo_corridor_input_lost();
        if (s_navigation.active == 1) demo_duel_input_lost(now);
        ESP_LOGW(TAG, "Input overflow; active game stopped");
    }
    game_input_t input;
    for (unsigned count = 0; count < 32 && xQueueReceive(s_input_queue, &input, 0) == pdTRUE; ++count) {
        if (input.generation == atomic_load(&s_generation)) process_key(&input);
    }
    if (s_navigation.active >= 0) {
        // Keep the batch timestamp: a fresh clock could overtake a press
        // enqueued after the last receive and make its timestamp stale.
        GAMES[s_navigation.active].tick(now);
    } else {
        game_launcher_update(s_navigation.selected, duel_io_battery());
    }
}

void app_main(void)
{
    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed");
        return;
    }
    bsp_display_backlight(100);
    // Blocking peripheral work happens before the UI event loop starts.
    const bool audio_ready = bsp_audio_init() == ESP_OK;
    const bool battery_ready = bsp_battery_init() == ESP_OK;
    if (!duel_io_init(audio_ready, battery_ready)) {
        ESP_LOGW(TAG, "Sound/battery worker unavailable");
    }
    demo_navigation_init(&s_navigation, GAME_COUNT);
    s_input_queue = xQueueCreate(32, sizeof(game_input_t));
    if (!s_input_queue) {
        ESP_LOGE(TAG, "Input queue allocation failed");
        return;
    }
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        ESP_LOGE(TAG, "Button initialization failed");
        return;
    }
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Unable to initialize game UI");
        return;
    }
    // Retain the released game's direct startup. Long OK in play returns to
    // its title; long OK on that title opens the independent game selector.
    s_navigation.active = 0;
    GAMES[0].enter();
    lv_timer_t *timer = lv_timer_create(process_input, 10, NULL);
    if (timer) atomic_store(&s_input_ready, true);
    else ESP_LOGE(TAG, "Input timer allocation failed");
    bsp_lvgl_unlock();
    if (fap_screenshot_start() != ESP_OK) ESP_LOGW(TAG, "Screenshot service unavailable");
}
