#include "duel_io.h"

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdint.h>

typedef struct {
    duel_cue_t cue;
    unsigned generation;
    bool enabled;
    bool music;
} sound_message_t;

static QueueHandle_t s_queue;
static atomic_uint s_generation;
static atomic_bool s_active;
static atomic_bool s_audio_ready;
static atomic_int s_battery = -1;
static bool s_battery_ready;

static void io_task(void *argument)
{
    (void)argument;
    int64_t last_battery_us = -INT64_C(5000000);
    duel_sound_t sound = {0};
    unsigned generation = 0;
    unsigned configured_generation = 0;
    for (;;) {
        sound_message_t message;
        const TickType_t wait = duel_sound_running(&sound) ? 0 : pdMS_TO_TICKS(100);
        if (xQueueReceive(s_queue, &message, wait) == pdTRUE) {
            generation = message.generation;
            duel_sound_set(&sound, message.enabled, message.music, message.cue);
        }
        if (!atomic_load(&s_active) || !atomic_load(&s_audio_ready) ||
            generation != atomic_load(&s_generation)) duel_sound_set(&sound, false, false, DUEL_CUE_NONE);
        if (duel_sound_running(&sound)) {
            if (configured_generation != generation) {
                if (bsp_audio_set_format(DUEL_SOUND_RATE, 16, 1) != ESP_OK) {
                    atomic_store(&s_audio_ready, false);
                    continue;
                }
                bsp_audio_set_volume(75);
                configured_generation = generation;
            }
            int16_t samples[128];
            duel_sound_render(&sound, samples, sizeof(samples) / sizeof(*samples));
            if (atomic_load(&s_active) && generation == atomic_load(&s_generation) &&
                bsp_audio_write(samples, sizeof(samples)) != ESP_OK) atomic_store(&s_audio_ready, false);
        }
        const int64_t now_us = esp_timer_get_time();
        if (s_battery_ready && atomic_load(&s_active) &&
            now_us - last_battery_us >= INT64_C(5000000)) {
            atomic_store(&s_battery, bsp_battery_soc());
            last_battery_us = now_us;
        }
    }
}

bool duel_io_init(bool audio_ready, bool battery_ready)
{
    if (s_queue) return true;
    atomic_store(&s_audio_ready, audio_ready);
    s_battery_ready = battery_ready;
    s_queue = xQueueCreate(1, sizeof(sound_message_t));
    if (!s_queue) return false;
    if (xTaskCreate(io_task, "duel_io", 3072, NULL, 3, NULL) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        atomic_store(&s_audio_ready, false);
        return false;
    }
    return true;
}

void duel_io_activate(bool active)
{
    atomic_store(&s_active, active);
    atomic_fetch_add(&s_generation, 1);
    if (s_queue) {
        const sound_message_t message = {.generation = atomic_load(&s_generation)};
        xQueueOverwrite(s_queue, &message);
    }
}

void duel_io_sound(bool enabled, bool music, duel_cue_t cue)
{
    if (!s_queue) return;
    const sound_message_t message = {.cue = cue, .generation = atomic_load(&s_generation),
                                      .enabled = enabled, .music = music};
    xQueueOverwrite(s_queue, &message);
}

int duel_io_battery(void)
{
    return atomic_load(&s_battery);
}

bool duel_io_audio_ready(void)
{
    return s_queue && atomic_load(&s_audio_ready);
}
