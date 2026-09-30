#include "duel_io.h"

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdint.h>

_Static_assert(EC_SOUND_RATE == DUEL_SOUND_RATE, "Shared codec sample rate");
/* LVGL port runs at priority 4. PCM must preempt rendering, then block in
 * the I2S write when the DMA ring is full; never busy-wait to pace audio. */
#define AUDIO_TASK_PRIORITY 5
/* Device stress testing left only 420 bytes with timing logs at 3072 bytes.
 * Reserve another 512 bytes for worker/codec call depth, not a larger PCM ring. */
#define AUDIO_TASK_STACK_BYTES 3584

typedef struct {
    bool corridor;
    ec_sound_scene_t scene;
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
    ec_sound_t corridor = {0};
    const ec_sound_scene_t silence = {0};
    bool is_corridor = false;
    unsigned generation = 0;
    unsigned configured_generation = 0;
    int64_t last_write_end=0, stats_start=0;
    unsigned samples_written=0, max_gap_us=0, max_render_us=0, max_write_us=0, peak_abs=0;
    for (;;) {
        sound_message_t message;
        const TickType_t wait = (duel_sound_running(&sound) || ec_sound_running(&corridor)) ? 0 : pdMS_TO_TICKS(100);
        if (xQueueReceive(s_queue, &message, wait) == pdTRUE) {
            if (generation != message.generation || is_corridor != message.corridor) {
                duel_sound_set(&sound, false, false, DUEL_CUE_NONE);
                ec_sound_set(&corridor, &silence);
            }
            generation = message.generation;
            is_corridor = message.corridor;
            if (is_corridor) ec_sound_set(&corridor, &message.scene);
            else duel_sound_set(&sound, message.enabled, message.music, message.cue);
        }
        if (!atomic_load(&s_active) || !atomic_load(&s_audio_ready) ||
            generation != atomic_load(&s_generation)) {
            duel_sound_set(&sound, false, false, DUEL_CUE_NONE);
            ec_sound_set(&corridor, &silence);
        }
        if (duel_sound_running(&sound) || ec_sound_running(&corridor)) {
            if (configured_generation != generation) {
                const int64_t setup_start=esp_timer_get_time();
                if (bsp_audio_set_format(DUEL_SOUND_RATE, 16, 1) != ESP_OK) {
                    atomic_store(&s_audio_ready, false);
                    ESP_LOGE("game_audio", "format failed generation=%u",generation);
                    continue;
                }
                bsp_audio_set_volume(75);
                configured_generation = generation;
                ESP_LOGI("game_audio", "ready generation=%u setup_us=%u",generation,(unsigned)(esp_timer_get_time()-setup_start));
            }
            const int64_t render_start=esp_timer_get_time();
            int16_t samples[128];
            if (is_corridor) {
                ec_sound_render(&corridor, samples, sizeof(samples) / sizeof(*samples));
                ec_sound_device_gain(samples, sizeof(samples) / sizeof(*samples));
            }
            else duel_sound_render(&sound, samples, sizeof(samples) / sizeof(*samples));
            for (unsigned i = 0; i < 128; ++i) {
                unsigned magnitude = samples[i] < 0 ? (unsigned)-(int)samples[i] : (unsigned)samples[i];
                if (magnitude > peak_abs) peak_abs = magnitude;
            }
            const int64_t write_start=esp_timer_get_time();
            unsigned render_us=(unsigned)(write_start-render_start);
            if(render_us>max_render_us)max_render_us=render_us;
            if(last_write_end){
                unsigned gap=(unsigned)(write_start-last_write_end);
                if(gap>max_gap_us)max_gap_us=gap;
            }
            if (atomic_load(&s_active) && generation == atomic_load(&s_generation)) {
                if(bsp_audio_write(samples, sizeof(samples)) != ESP_OK){
                    atomic_store(&s_audio_ready, false);
                    ESP_LOGE("game_audio", "write failed generation=%u",generation);
                }else samples_written+=128;
            }
            last_write_end=esp_timer_get_time();
            unsigned write_us=(unsigned)(last_write_end-write_start);
            if(write_us>max_write_us)max_write_us=write_us;
            if(!stats_start)stats_start=write_start;
            if(last_write_end-stats_start>=5000000){
                ESP_LOGI("game_audio", "rate=%u gap_max_us=%u render_max_us=%u write_max_us=%u stack_free=%u peak=%u steps=%u",
                    (unsigned)((uint64_t)samples_written*1000000/(last_write_end-stats_start)),max_gap_us,max_render_us,max_write_us,
                    (unsigned)uxTaskGetStackHighWaterMark(NULL),peak_abs,(unsigned)corridor.scene.steps);
                stats_start=last_write_end;samples_written=max_gap_us=max_render_us=max_write_us=peak_abs=0;
            }
        }else{
            if (last_write_end) ESP_LOGI("game_audio", "idle generation=%u", generation);
            last_write_end=stats_start=0;samples_written=max_gap_us=max_render_us=max_write_us=peak_abs=0;
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
    /* Battery setup has already completed in app_main. Publish the first SOC
     * before the game title is drawn; the worker handles later readings. */
    atomic_store(&s_battery, battery_ready ? bsp_battery_soc() : -1);
    s_queue = xQueueCreate(1, sizeof(sound_message_t));
    if (!s_queue) return false;
    if (xTaskCreate(io_task, "duel_io", AUDIO_TASK_STACK_BYTES, NULL, AUDIO_TASK_PRIORITY, NULL) != pdPASS) {
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

void duel_io_corridor_sound(const ec_sound_scene_t *scene)
{
    if (!s_queue || !scene) return;
    const sound_message_t message = {.corridor = true, .scene = *scene,
                                     .generation = atomic_load(&s_generation)};
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
