// No LVGL task may see the display until the rounding callback is registered.
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Forward declare stubs before including implementation
static int panel_present = 1, io_present = 1;
static int lock_depth, port_live, display_live, callback_live, flush_wait_cb_installed;
static int fail_lock, fail_port, fail_display, fail_event, fail_sem_create;
static int refresh_deleted;
static int fail_register_io, fail_unregister_io;
static int sem_created_count, sem_deleted_count, sem_tokens;
static int simulate_late_give, late_give_occurred;
static int init_calls, unlocked_flushes;
static int panel_token, io_token;
static void *active_io_cb_ctx;
static esp_lcd_panel_io_color_trans_done_cb_t active_io_cb;
static lv_display_flush_wait_cb_t installed_flush_wait_cb;

static int fake_sem_handle = 0x1234;

SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    if (fail_sem_create) return NULL;
    ++sem_created_count;
    sem_tokens = 0;
    return (SemaphoreHandle_t)&fake_sem_handle;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *pxHigherPriorityTaskWoken) {
    assert(sem == (SemaphoreHandle_t)&fake_sem_handle);
    assert(sem_created_count > sem_deleted_count); // Cannot give to deleted semaphore
    if (sem_tokens == 0) {
        sem_tokens = 1;
        if (pxHigherPriorityTaskWoken) *pxHigherPriorityTaskWoken = pdTRUE;
        return pdTRUE;
    }
    if (pxHigherPriorityTaskWoken) *pxHigherPriorityTaskWoken = pdFALSE;
    return pdFAIL;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t xTicksToWait) {
    assert(sem == (SemaphoreHandle_t)&fake_sem_handle);
    assert(sem_created_count > sem_deleted_count); // Cannot take from deleted semaphore
    assert(xTicksToWait == portMAX_DELAY || xTicksToWait == 0);

    if (simulate_late_give && sem_tokens == 0 && active_io_cb) {
        // Simulate DMA interrupt firing while task is blocked in wait
        assert(xTicksToWait == portMAX_DELAY);
        BaseType_t high_task_woken = pdFALSE;
        bool woken = active_io_cb(&io_token, NULL, active_io_cb_ctx);
        assert(woken == true);
        (void)high_task_woken;
        late_give_occurred = 1;
    }

    if (sem_tokens > 0) {
        --sem_tokens;
        return pdTRUE;
    }
    return pdFALSE;
}

void vSemaphoreDelete(SemaphoreHandle_t sem) {
    assert(sem == (SemaphoreHandle_t)&fake_sem_handle);
    assert(sem_created_count > sem_deleted_count);
    ++sem_deleted_count;
    sem_tokens = 0;
}

esp_err_t esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t io,
                                                     const esp_lcd_panel_io_callbacks_t *cbs,
                                                     void *user_ctx) {
    assert(io == &io_token);
    if (cbs && cbs->on_color_trans_done) {
        if (fail_register_io) return ESP_FAIL;
        active_io_cb = cbs->on_color_trans_done;
        active_io_cb_ctx = user_ctx;
        return ESP_OK;
    }
    // Unregister path (empty callbacks or null callback)
    if (fail_unregister_io) return ESP_FAIL;
    active_io_cb = NULL;
    active_io_cb_ctx = NULL;
    return ESP_OK;
}

void lv_display_set_flush_wait_cb(lv_display_t *disp, lv_display_flush_wait_cb_t wait_cb) {
    (void)disp;
    installed_flush_wait_cb = wait_cb;
    flush_wait_cb_installed = (wait_cb != NULL);
}

#include "../components/bsp/src/bsp_display_lvgl.c"

static lv_display_t display;

esp_lcd_panel_handle_t bsp_display_panel(void) { return panel_present ? &panel_token : NULL; }
esp_lcd_panel_io_handle_t bsp_display_io(void) { return io_present ? &io_token : NULL; }

esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg) {
    (void)cfg; ++init_calls; assert(!port_live);
    if (fail_port) return ESP_ERR_NO_MEM;
    port_live = 1; return ESP_OK;
}

esp_err_t lvgl_port_deinit(void) {
    // The real API is asynchronous: returning does not release the context.
    assert(false && "Display rollback must not deinit/reinitialize the live port");
    return ESP_OK;
}

bool lvgl_port_lock(uint32_t timeout) {
    (void)timeout; assert(port_live);
    if (fail_lock) return false;
    ++lock_depth; return true;
}

void lvgl_port_unlock(void) {
    assert(lock_depth > 0);
    --lock_depth;
    if (!lock_depth && display_live && !refresh_deleted) {
        assert(callback_live); // Simulate rendering as soon as the lock is free.
        ++unlocked_flushes;
    }
}

lv_display_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *cfg) {
    assert(lock_depth > 0 && cfg->panel_handle == &panel_token && !display_live);
    assert(lvgl_port_lock(0)); // Real port takes and releases a recursive lock.
    if (!fail_display) {
        display_live = 1;
        refresh_deleted = 0;
        // esp_lvgl_port registers its own default completion callback referencing disp
        active_io_cb = (esp_lcd_panel_io_color_trans_done_cb_t)0x5678;
        active_io_cb_ctx = &display;
    }
    lvgl_port_unlock();
    return fail_display ? NULL : &display;
}

void lv_display_delete_refr_timer(lv_display_t *disp) {
    assert(disp == &display && lock_depth);
    refresh_deleted = 1;
}

esp_err_t lvgl_port_remove_disp(lv_display_t *disp) {
    assert(disp == &display && lock_depth && display_live);
    // Real safety check: when display is removed, active_io_cb must NOT point to disp!
    assert(active_io_cb_ctx != &display && "Cannot remove display while IO callback still references it!");
    display_live = callback_live = 0;
    return ESP_OK;
}

void lv_display_add_event_cb(lv_display_t *disp, void (*cb)(lv_event_t *), int code, void *user) {
    (void)user;
    assert(lock_depth && disp == &display && code == LV_EVENT_FLUSH_START && cb == rounded_flush_event);
    if (!fail_event) callback_live = 1;
}

uint32_t lv_display_get_event_count(lv_display_t *disp) {
    assert(disp == &display && lock_depth);
    return 1 + callback_live; // The display already owns an internal callback.
}

static void expect_failure(void) {
    assert(bsp_lvgl_init() == NULL);
    assert(!s_disp && !lock_depth && !display_live);
    assert(!bsp_lvgl_lock(0));
}

static void expect_retained_failure(void) {
    assert(bsp_lvgl_init() == NULL);
    assert(s_port_init_failed && !s_disp && !lock_depth);
    assert(display_live && refresh_deleted);
    assert(sem_created_count > sem_deleted_count);
    int before = init_calls;
    assert(bsp_lvgl_init() == NULL && init_calls == before);
    assert(!bsp_lvgl_lock(0));
}

int main(void) {
    // 1. Missing panel or IO handle
    panel_present = 0; expect_failure(); panel_present = 1;
    io_present = 0; expect_failure(); io_present = 1;

    // 2. Port init failure
    fail_port = 1; expect_failure(); fail_port = 0;
    const int failed_init_calls = init_calls;
    expect_failure(); // A partially initialized port cannot safely be re-created.
    assert(init_calls == failed_init_calls);
    s_port_init_failed = false; // Simulate reboot for remaining scenarios.

    fail_sem_create = 1; expect_failure(); fail_sem_create = 0;
    assert(sem_created_count == sem_deleted_count);

    // 3. Lock failure
    fail_lock = 1; expect_failure(); fail_lock = 0;
    assert(sem_created_count == sem_deleted_count);

    // 4. Display allocation failure
    const int retained_init_calls = init_calls;
    fail_display = 1; expect_failure(); fail_display = 0;
    assert(sem_created_count == sem_deleted_count);

    // 5. Custom IO callback registration failure:
    // esp_lvgl_port already registered a default callback referencing disp;
    // failure path must unregister that callback before removing disp.
    fail_register_io = 1; expect_failure(); fail_register_io = 0;
    assert(sem_created_count == sem_deleted_count);
    assert(!active_io_cb && !active_io_cb_ctx); // Cleared default callback

    // 6. Custom IO callback registration failure WITH unregister failure:
    // Default callback could not be cleared; must retain display/semaphore and mark unrecoverable.
    fail_register_io = 1;
    fail_unregister_io = 1;
    expect_retained_failure();
    fail_register_io = 0;
    fail_unregister_io = 0;
    assert(s_port_init_failed == true);
    // In unrecoverable state, display was kept so ISR won't crash
    display_live = 0; // manually clean up mock for subsequent test cases
    s_port_init_failed = false; // reset mock reboot
    if (s_flush_sem) { vSemaphoreDelete(s_flush_sem); s_flush_sem = NULL; }
    active_io_cb = NULL;
    active_io_cb_ctx = NULL;
    s_io_cb_live = false;
    installed_flush_wait_cb = NULL;
    flush_wait_cb_installed = 0;

    // 7. Event callback registration failure (normal unregister success)
    fail_event = 1; expect_failure(); fail_event = 0;
    assert(sem_created_count == sem_deleted_count);
    assert(!active_io_cb && !active_io_cb_ctx);
    assert(!flush_wait_cb_installed);

    // 8. Event callback registration failure WITH unregister failure:
    // Custom callback remains active in hardware; semaphore & display must NOT be freed!
    fail_event = 1;
    fail_unregister_io = 1;
    expect_retained_failure();
    fail_event = 0;
    fail_unregister_io = 0;
    assert(s_port_init_failed == true);
    display_live = 0;
    s_port_init_failed = false;
    if (s_flush_sem) { vSemaphoreDelete(s_flush_sem); s_flush_sem = NULL; }
    active_io_cb = NULL;
    active_io_cb_ctx = NULL;
    s_io_cb_live = false;
    installed_flush_wait_cb = NULL;
    flush_wait_cb_installed = 0;

    // 9. Successful initialization
    assert(bsp_lvgl_init() == &display);
    assert(init_calls == retained_init_calls); // Display retries reuse the port.
    assert(callback_live && !lock_depth && unlocked_flushes == 1);
    assert(flush_wait_cb_installed);
    assert(active_io_cb == bsp_lvgl_color_trans_done_cb);
    assert(active_io_cb_ctx == (void *)s_flush_sem);

    // Multiple init calls return existing display without re-initializing
    const int before = init_calls;
    assert(bsp_lvgl_init() == &display && init_calls == before);
    assert(bsp_lvgl_lock(5)); bsp_lvgl_unlock();

    // 10. Verify wait and give semantics:
    // Case A: Early give (DMA finishes before wait_for_flushing calls flush_wait_cb)
    assert(sem_tokens == 0);
    bool woken = active_io_cb(&io_token, NULL, active_io_cb_ctx);
    assert(woken == true);
    assert(sem_tokens == 1);
    // When LVGL calls flush_wait_cb, token must be consumed without blocking
    installed_flush_wait_cb(&display);
    assert(sem_tokens == 0);

    // Case B: Late give (simulate DMA ISR arriving WHILE task is blocked in xSemaphoreTake)
    assert(sem_tokens == 0);
    simulate_late_give = 1;
    late_give_occurred = 0;
    installed_flush_wait_cb(&display);
    assert(late_give_occurred == 1);
    assert(sem_tokens == 0);
    simulate_late_give = 0;

    // Verify giving when already full does not corrupt token count
    woken = active_io_cb(&io_token, NULL, active_io_cb_ctx);
    assert(woken == true && sem_tokens == 1);
    // Second give while full returns false (no task woken) and stays at 1
    woken = active_io_cb(&io_token, NULL, active_io_cb_ctx);
    assert(woken == false && sem_tokens == 1);
    installed_flush_wait_cb(&display);
    assert(sem_tokens == 0);

    // 11. Verify rounding mask functionality
    uint16_t pixels[BSP_LCD_W] = {0};
    for (int x = 0; x < BSP_LCD_W; ++x) pixels[x] = 0xffff;
    display.buffer = (lv_draw_buf_t){ .data = (uint8_t *)pixels, .header.stride = sizeof(pixels) };
    lv_area_t area = { .x1 = 0, .y1 = 0, .x2 = BSP_LCD_W - 1, .y2 = 0 };
    lv_event_t ev = { .target = &display, .area = &area };
    rounded_flush_event(&ev);
    assert(pixels[0] == 0 && pixels[BSP_LCD_W - 1] == 0 && pixels[BSP_LCD_W / 2] == 0xffff);

    puts("BSP LVGL initialization tests: PASS");
    return 0;
}
