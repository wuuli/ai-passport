#include "duel_sound.h"

typedef struct {
    unsigned frequency;
    unsigned duration_ms;
} note_t;

static const unsigned s_melody[] = {
    440, 0, 659, 440, 784, 659, 523, 0,
    587, 0, 698, 587, 880, 784, 659, 0,
    523, 659, 784, 0, 698, 587, 440, 0,
    494, 587, 659, 784, 659, 494, 440, 0,
};
static const unsigned s_bass[] = {110, 147, 131, 165};
static const note_t s_button[] = {{1047, 28}};
static const note_t s_start[] = {{784, 24}};
static const note_t s_stop[] = {{523, 40}};
static const note_t s_win[] = {{659, 80}, {784, 80}, {988, 120}};
static const note_t s_lose[] = {{392, 100}, {330, 100}, {262, 120}};
static const note_t s_match[] = {{523, 90}, {659, 90}, {784, 90}, {0, 60},
                                {784, 90}, {1047, 220}};

static note_t cue_note(duel_cue_t cue, unsigned index)
{
    const note_t *notes = NULL;
    size_t count = 0;
    switch (cue) {
    case DUEL_CUE_BUTTON: notes = s_button; count = sizeof(s_button) / sizeof(*notes); break;
    case DUEL_CUE_START: notes = s_start; count = sizeof(s_start) / sizeof(*notes); break;
    case DUEL_CUE_STOP: notes = s_stop; count = sizeof(s_stop) / sizeof(*notes); break;
    case DUEL_CUE_WIN: notes = s_win; count = sizeof(s_win) / sizeof(*notes); break;
    case DUEL_CUE_LOSE: notes = s_lose; count = sizeof(s_lose) / sizeof(*notes); break;
    case DUEL_CUE_MATCH: notes = s_match; count = sizeof(s_match) / sizeof(*notes); break;
    default: break;
    }
    return index < count ? notes[index] : (note_t){0};
}

static int voice(unsigned *phase, unsigned frequency, unsigned position,
                 unsigned duration, int amplitude)
{
    if (!frequency || position >= duration) return 0;
    *phase = (*phase + frequency) % DUEL_SOUND_RATE;
    const int triangle = *phase < 8000 ? (int)*phase * 2 - 8000 : 24000 - (int)*phase * 2;
    unsigned envelope = 64;
    if (position < envelope) envelope = position;
    if (duration - position < envelope) envelope = duration - position;
    return triangle * amplitude / 8000 * (int)envelope / 64;
}

void duel_sound_set(duel_sound_t *sound, bool enabled, bool music, duel_cue_t cue)
{
    if (!enabled) {
        *sound = (duel_sound_t){0};
        return;
    }
    sound->enabled = true;
    sound->music = music;
    sound->cue = cue >= DUEL_CUE_NONE && cue <= DUEL_CUE_MATCH ? cue : DUEL_CUE_NONE;
    sound->cue_note = 0;
    sound->cue_sample = 0;
    sound->cue_phase = 0;
}

bool duel_sound_running(const duel_sound_t *sound)
{
    return sound->enabled && (sound->music || sound->cue != DUEL_CUE_NONE);
}

void duel_sound_render(duel_sound_t *sound, int16_t *samples, size_t count)
{
    for (size_t index = 0; index < count; ++index) {
        int sample = 0;
        if (sound->enabled && sound->music) {
            const unsigned step = sound->music_sample / 3200;
            const unsigned position = sound->music_sample % 3200;
            sample += voice(&sound->melody_phase, s_melody[step], position, 2700, 1540);
            sample += voice(&sound->bass_phase, s_bass[step / 8], position, 1600, 770);
            sound->music_sample = (sound->music_sample + 1) % (32 * 3200);
        }
        if (sound->enabled && sound->cue != DUEL_CUE_NONE) {
            const note_t note = cue_note(sound->cue, sound->cue_note);
            const unsigned duration = note.duration_ms * (DUEL_SOUND_RATE / 1000);
            sample += voice(&sound->cue_phase, note.frequency, sound->cue_sample, duration, 2600);
            if (++sound->cue_sample >= duration) {
                sound->cue_sample = 0;
                sound->cue_phase = 0;
                if (!cue_note(sound->cue, ++sound->cue_note).duration_ms) sound->cue = DUEL_CUE_NONE;
            }
        }
        samples[index] = (int16_t)sample;
    }
}
