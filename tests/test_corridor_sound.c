#include "corridor_sound.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void tick(ec_game_t *g, ec_sound_tracker_t *t, unsigned n)
{
    while(n--){ec_game_tick(g,.05f);ec_sound_track(t,g);}
}
static void test_movement(void)
{
    ec_game_t g; ec_game_init(&g,42);
    ec_sound_tracker_t t; ec_sound_track_init(&t,&g);
    assert(!t.scene.enabled);
    ec_game_key(&g,EC_OK);ec_sound_track(&t,&g);
    assert(t.scene.enabled && t.scene.ambient);
    tick(&g,&t,20);assert(t.scene.steps==0);
    ec_game_key(&g,EC_OK);ec_sound_track(&t,&g);
    tick(&g,&t,40);assert(t.scene.steps>=3);
    ec_game_key(&g,EC_OK);ec_sound_track(&t,&g);
    unsigned steps=t.scene.steps;
    tick(&g,&t,30);assert(t.scene.steps==steps);
    ec_game_key(&g,EC_RIGHT);ec_sound_track(&t,&g);
    tick(&g,&t,20);assert(t.scene.steps==steps);
    g.observing=true;g.z+=.9f;ec_sound_track(&t,&g);
    assert(t.scene.steps==steps);
    g.observing=false;g.walking=true;ec_sound_track(&t,&g);
    ++g.passages;g.x-=9.6f;g.z+=29.6f;ec_sound_track(&t,&g);
    assert(t.scene.steps==steps);
    /* Real automatic arc contributes footsteps and stops after the corner. */
    ec_game_init(&g,42);g.phase=EC_PLAYING;g.z=-25.1f;g.walking=true;
    ec_sound_track_init(&t,&g);tick(&g,&t,100);
    assert(g.corner_stop && !g.walking && t.scene.steps>=2);
    steps=t.scene.steps;tick(&g,&t,40);assert(t.scene.steps==steps);
    g.phase=EC_EXITING;ec_sound_track(&t,&g);
    g.phase=EC_CLEARED;ec_sound_track(&t,&g);
    assert(t.scene.clears==1 && !t.scene.ambient);
    ec_sound_track(&t,&g);assert(t.scene.clears==1);
    ec_game_key(&g,EC_OK);ec_sound_track(&t,&g);
    assert(t.scene.steps==0 && t.scene.clears==0);
    ec_game_init(&g,42);ec_sound_track(&t,&g);assert(!t.scene.enabled);
}

static void test_pcm(void)
{
    ec_sound_t a={0},b={0};int16_t pcm[640],split[640];
    ec_sound_render(&a,pcm,640);for(unsigned i=0;i<640;i++)assert(!pcm[i]);
    ec_sound_scene_t scene={.enabled=true,.ambient=true};
    ec_sound_set(&a,&scene);ec_sound_set(&b,&scene);
    for(unsigned frame=0;frame<100;frame++){
        if(frame%10==0)++scene.steps;
        ec_sound_set(&a,&scene);ec_sound_set(&b,&scene);
        ec_sound_render(&a,pcm,640);
        for(unsigned i=0;i<5;i++)ec_sound_render(&b,split+i*128,128);
        assert(memcmp(pcm,split,sizeof(pcm))==0);
        bool nonzero=false;
        for(unsigned i=0;i<640;i++){assert(pcm[i]<=6000 && pcm[i]>=-6000);nonzero|=pcm[i]!=0;}
        assert(nonzero);
    }
    /* Overwrite/coalesced events play one recent footstep, never a backlog. */
    scene.steps+=50;ec_sound_set(&a,&scene);assert(a.step_left);
    ec_sound_render(&a,pcm,640);unsigned left=a.step_left;
    ec_sound_set(&a,&scene);assert(a.step_left==left);
    scene.ambient=false;++scene.clears;ec_sound_set(&a,&scene);
    assert(a.clear_left && !a.step_left);
    ec_sound_render(&a,pcm,640);left=a.clear_left;
    ec_sound_set(&a,&scene);assert(a.clear_left==left);
    for(unsigned i=0;i<40;i++)ec_sound_render(&a,pcm,640);
    assert(!ec_sound_running(&a));
    ec_sound_scene_t silence={0};ec_sound_set(&a,&silence);
    ec_sound_render(&a,pcm,640);for(unsigned i=0;i<640;i++)assert(!pcm[i]);
    ec_sound_set(&a,&scene);assert(!a.step_left && !a.clear_left); /* resume: no history */
    scene.ambient=true;scene.steps=scene.clears=0;ec_sound_set(&a,&scene);
    assert(!a.clear_left && !a.step_left && ec_sound_running(&a));
    assert(sizeof(a)<128 && sizeof(ec_sound_tracker_t)<128);
}

static void test_device_gain(void)
{
    int16_t limits[]={0,1,-1,5461,-5461,5462,-5462,INT16_MAX,INT16_MIN};
    const int16_t expected[]={0,6,-6,32766,-32766,32767,-32768,32767,-32768};
    ec_sound_device_gain(limits,sizeof(limits)/sizeof(*limits));
    assert(memcmp(limits,expected,sizeof(limits))==0);
    /* Real ambience, alternating steps and completion remain below clipping.
     * Device PCM matches the browser's gain at 60% for this full sequence. */
    ec_sound_t sound={0};
    ec_sound_scene_t scene={.enabled=true,.ambient=true};
    int16_t raw[640],device[640];
    ec_sound_set(&sound,&scene);
    for (unsigned frame=0;frame<300;++frame) {
        if (frame<250 && frame%10==0) ++scene.steps;
        if (frame==250) { scene.ambient=false; ++scene.clears; }
        ec_sound_set(&sound,&scene);
        ec_sound_render(&sound,raw,640);
        memcpy(device,raw,sizeof(raw));
        ec_sound_device_gain(device,640);
        for (unsigned i=0;i<640;++i) {
            int32_t amplified=(int32_t)raw[i]*6;
            assert(amplified>INT16_MIN && amplified<INT16_MAX);
            assert(device[i]==amplified);
        }
    }
}

int main(void)
{
    test_movement();test_pcm();test_device_gain();
    printf("Corridor sound PASS: synth=%zu B tracker=%zu B, no PCM assets\n",sizeof(ec_sound_t),sizeof(ec_sound_tracker_t));
    return 0;
}
