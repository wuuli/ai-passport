#include "duel_sound.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool audible(duel_sound_t *sound, unsigned blocks)
{
    bool heard = false;
    for (unsigned block = 0; block < blocks; ++block) {
        int16_t samples[128];
        duel_sound_render(sound, samples, 128);
        for (unsigned index = 0; index < 128; ++index) {
            assert(samples[index] >= -4910 && samples[index] <= 4910);
            heard |= samples[index] != 0;
        }
    }
    return heard;
}

static void test_music_and_mute(void)
{
    duel_sound_t sound = {0};
    assert(!duel_sound_running(&sound));
    assert(!audible(&sound, 2));
    duel_sound_set(&sound, true, true, DUEL_CUE_NONE);
    assert(duel_sound_running(&sound));
    assert(audible(&sound, 2000));
    duel_sound_set(&sound, false, true, DUEL_CUE_MATCH);
    assert(!duel_sound_running(&sound));
    assert(!audible(&sound, 100));
}

static void test_all_cues_and_timing_silence(void)
{
    for (duel_cue_t cue = DUEL_CUE_BUTTON; cue <= DUEL_CUE_MATCH; ++cue) {
        duel_sound_t sound = {0};
        duel_sound_set(&sound, true, false, cue);
        assert(audible(&sound, 125));
        assert(!duel_sound_running(&sound));
        assert(!audible(&sound, 125));
    }
    duel_sound_t sound = {0};
    duel_sound_set(&sound, true, true, DUEL_CUE_WIN);
    assert(audible(&sound, 5));
    duel_sound_set(&sound, true, false, DUEL_CUE_START);
    assert(audible(&sound, 3));
    assert(!duel_sound_running(&sound));
    assert(!audible(&sound, 750));
    duel_sound_set(&sound, true, true, DUEL_CUE_STOP);
    assert(audible(&sound, 100));
}

static void test_preemption_and_chunk_independence(void)
{
    duel_sound_t first = {0};
    duel_sound_set(&first, true, true, DUEL_CUE_MATCH);
    assert(audible(&first, 2));
    duel_sound_set(&first, true, false, DUEL_CUE_BUTTON);
    assert(audible(&first, 4));
    assert(!audible(&first, 100));
    duel_sound_set(&first, true, false, (duel_cue_t)99);
    assert(!duel_sound_running(&first));
    duel_sound_set(&first, true, true, DUEL_CUE_MATCH);
    duel_sound_t second = first;
    int16_t whole[512];
    int16_t chunks[512];
    duel_sound_render(&first, whole, 512);
    for (unsigned offset = 0; offset < 512; offset += 64) {
        duel_sound_render(&second, chunks + offset, 64);
    }
    assert(memcmp(whole, chunks, sizeof(whole)) == 0);
}

static void test_cue_waveforms_unchanged(void)
{
    const uint32_t expected[] = {UINT32_C(0xd3596de0), UINT32_C(0x65fe8141),
        UINT32_C(0xb33d176b), UINT32_C(0x125254b3), UINT32_C(0x6d30456c), UINT32_C(0x58c67a75)};
    for (duel_cue_t cue = DUEL_CUE_BUTTON; cue <= DUEL_CUE_MATCH; ++cue) {
        duel_sound_t sound = {0};
        duel_sound_set(&sound, true, false, cue);
        uint32_t hash = UINT32_C(2166136261);
        for (unsigned block = 0; block < 125; ++block) {
            int16_t samples[128];
            duel_sound_render(&sound, samples, 128);
            for (unsigned index = 0; index < 128; ++index) {
                hash = (hash ^ ((uint16_t)samples[index] & 255u)) * UINT32_C(16777619);
                hash = (hash ^ ((uint16_t)samples[index] >> 8)) * UINT32_C(16777619);
            }
        }
        assert(hash == expected[cue - DUEL_CUE_BUTTON]);
    }
}

int main(void)
{
    test_music_and_mute();
    test_all_cues_and_timing_silence();
    test_preemption_and_chunk_independence();
    test_cue_waveforms_unchanged();
    printf("Time Challenge sound: music, mute, six unchanged cues, preemption, timing silence PASS (%zu bytes)\n", sizeof(duel_sound_t));
    return 0;
}
