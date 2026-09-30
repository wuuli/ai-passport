#include "corridor_sound.h"
#include <math.h>
#include <string.h>

#define STEP_DISTANCE .85f
#define STEP_SAMPLES 3520u
#define CLEAR_SAMPLES 19200u
#define FADE_SAMPLES 4096u

static bool in_corridor(ec_phase_t phase)
{
    return phase == EC_PLAYING || phase == EC_EXITING;
}

void ec_sound_track_init(ec_sound_tracker_t *t, const ec_game_t *g)
{
    *t = (ec_sound_tracker_t){.x=g->x, .z=g->z, .phase=g->phase,
        .passages=g->passages, .walking=g->walking, .observing=g->observing,
        .scene={.enabled=g->phase!=EC_TITLE, .ambient=in_corridor(g->phase)}};
}

void ec_sound_track(ec_sound_tracker_t *t, const ec_game_t *g)
{
    if (g->phase == EC_TITLE || (t->phase == EC_CLEARED && g->phase == EC_PLAYING)) {
        ec_sound_track_init(t,g);
        return;
    }
    /* Observation easing is not walking. A portal remaps world coordinates;
     * never interpret that jump as a burst of footsteps. */
    if (in_corridor(t->phase) && in_corridor(g->phase) &&
        (t->walking || g->walking) && !t->observing && !g->observing &&
        g->passages == t->passages) {
        float dx=g->x-t->x, dz=g->z-t->z;
        float distance=sqrtf(dx*dx+dz*dz);
        if (distance < 1.0f) {
            t->distance += distance;
            if (t->distance >= STEP_DISTANCE) {
                ++t->scene.steps;
                t->distance -= STEP_DISTANCE;
            }
        }
    }
    if (t->phase == EC_EXITING && g->phase == EC_CLEARED) ++t->scene.clears;
    t->scene.enabled=g->phase!=EC_TITLE;
    t->scene.ambient=in_corridor(g->phase);
    t->x=g->x; t->z=g->z; t->phase=g->phase;
    t->passages=g->passages; t->walking=g->walking; t->observing=g->observing;
}

void ec_sound_set(ec_sound_t *s, const ec_sound_scene_t *scene)
{
    if (!scene->enabled) {
        memset(s,0,sizeof(*s));
        return;
    }
    if (!s->scene.enabled || scene->steps < s->scene.steps || scene->clears < s->scene.clears) {
        /* Enabling/replaying baselines counters, so muted history is not heard. */
        *s=(ec_sound_t){.scene=*scene, .noise=0x6d2b79f5u};
        return;
    }
    if (scene->steps != s->scene.steps) {
        s->step_left=STEP_SAMPLES; s->shoe=0;
    }
    if (scene->clears != s->scene.clears) {
        s->clear_left=CLEAR_SAMPLES; s->clear_phase=0; s->step_left=0;
    }
    s->scene=*scene;
}

bool ec_sound_running(const ec_sound_t *s)
{
    return s->scene.enabled && (s->scene.ambient || s->ambience || s->step_left || s->clear_left);
}

static int triangle(unsigned *phase, unsigned hz)
{
    *phase=(*phase+hz)%EC_SOUND_RATE;
    return *phase<8000 ? (int)*phase*2-8000 : 24000-(int)*phase*2;
}

/* Small band-limited body modes avoid the buzzy harmonics of a triangle.
 * A soft heel contact, later sole contact and one quiet reflection replace the
 * previous long broadband hiss. Table lives in Flash, no PCM heap or delay line. */
static const int16_t sine[128]={
    0, 1608, 3212, 4808, 6393, 7962, 9512, 11039, 12539, 14010, 15446, 16846, 18204, 19519, 20787, 22005,
    23170, 24279, 25329, 26319, 27245, 28105, 28898, 29621, 30273, 30852, 31356, 31785, 32137, 32412, 32609, 32728,
    32767, 32728, 32609, 32412, 32137, 31785, 31356, 30852, 30273, 29621, 28898, 28105, 27245, 26319, 25329, 24279,
    23170, 22005, 20787, 19519, 18204, 16846, 15446, 14010, 12539, 11039, 9512, 7962, 6393, 4808, 3212, 1608,
    0, -1608, -3212, -4808, -6393, -7962, -9512, -11039, -12539, -14010, -15446, -16846, -18204, -19519, -20787, -22005,
    -23170, -24279, -25329, -26319, -27245, -28105, -28898, -29621, -30273, -30852, -31356, -31785, -32137, -32412, -32609, -32728,
    -32767, -32728, -32609, -32412, -32137, -31785, -31356, -30852, -30273, -29621, -28898, -28105, -27245, -26319, -25329, -24279,
    -23170, -22005, -20787, -19519, -18204, -16846, -15446, -14010, -12539, -11039, -9512, -7962, -6393, -4808, -3212, -1608
};
static int body(unsigned time, unsigned hz)
{
    return sine[(time*hz*128u/EC_SOUND_RATE)&127u];
}
static int envelope(unsigned time, unsigned length)
{
    if(time>=length)return 0;
    int fade=(int)((length-time)*256u/length);
    int attack=time<48u?(int)(time*256u/48u):256;
    return fade*fade/256*attack/256;
}
static int foot_contact(unsigned time, unsigned pitch, int texture)
{
    int weight=(body(time,pitch)/32+body(time,213)/96)*envelope(time,1440)/256;
    int contact=texture/12*envelope(time,320)/256;
    return weight+contact;
}

void ec_sound_device_gain(int16_t *samples, size_t count)
{
    for (size_t i=0;i<count;++i) {
        int32_t sample=(int32_t)samples[i]*6;
        if (sample>INT16_MAX) sample=INT16_MAX;
        if (sample<INT16_MIN) sample=INT16_MIN;
        samples[i]=(int16_t)sample;
    }
}

void ec_sound_render(ec_sound_t *s, int16_t *samples, size_t count)
{
    for (size_t i=0;i<count;++i) {
        if (!ec_sound_running(s)) { samples[i]=0; continue; }
        s->noise ^= s->noise<<13; s->noise ^= s->noise>>17; s->noise ^= s->noise<<5;
        int noise=(int)(s->noise&65535u)-32768;
        s->air += (noise-s->air)/32;
        if (s->scene.ambient && s->ambience<FADE_SAMPLES) ++s->ambience;
        else if (!s->scene.ambient && s->ambience) --s->ambience;
        /* Stationary ambience must sit well below foot contacts. A constant
         * pitched lamp buzz became intrusive after desktop gain calibration. */
        int sample=(s->air/80)*(int)s->ambience/(int)FADE_SAMPLES;
        if (s->step_left) {
            unsigned elapsed=STEP_SAMPLES-s->step_left;
            s->shoe += (noise-s->shoe)/12;
            unsigned pitch=(s->scene.steps&1u)?116:109;
            int foot=foot_contact(elapsed,pitch,s->shoe);
            if(elapsed>=512)foot+=foot_contact(elapsed-512,pitch+17,s->shoe)/3;
            if(elapsed>=960)foot+=foot_contact(elapsed-960,pitch,s->air)/5;
            sample += foot;
            --s->step_left;
        }
        if (s->clear_left) {
            unsigned elapsed=CLEAR_SAMPLES-s->clear_left;
            unsigned attack=elapsed<800 ? elapsed : 800;
            int tone=triangle(&s->clear_phase,523)/8;
            sample += tone*(int)s->clear_left/(int)CLEAR_SAMPLES*(int)attack/800;
            --s->clear_left;
        }
        if (sample>6000) sample=6000;
        if (sample< -6000) sample=-6000;
        samples[i]=(int16_t)sample;
    }
}
