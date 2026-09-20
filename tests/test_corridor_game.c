#include <string.h>
#include "corridor_game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define PI 3.14159265358979323846f
static void until_stop(ec_game_t *g){for(int i=0;i<2000&&g->walking;++i)ec_game_tick(g,.05f);assert(!g->walking);}
static void turn(ec_game_t *g,int n){for(int i=0;i<n;++i)ec_game_key(g,EC_RIGHT);}
static void portal(ec_game_t *g,bool exit_side){
    g->x=exit_side?4.79f:-4.79f;g->z=exit_side?-26.8f:2.8f;
    g->yaw=exit_side?PI/2:-PI/2;g->walking=true;ec_game_tick(g,.1f);
}
static void tick_for(ec_game_t *g, float dur){
    for(float t=0;t<dur&&(g->walking||g->turning);t+=.05f)ec_game_tick(g,.05f);
}

int main(void){
    ec_game_t g;ec_game_init(&g,123);assert(g.phase==EC_TITLE&&!g.walking);
    assert(ec_game_exit_progress(&g)==0.0f&&fabsf(ec_game_eye_height(&g)-1.55f)<.001f);
    ec_game_key(&g,EC_OK);assert(g.phase==EC_PLAYING&&g.anomaly==EC_NORMAL&&!g.walking);
    assert(ec_game_exit_progress(&g)==0.0f&&fabsf(ec_game_eye_height(&g)-1.55f)<.001f);
    ec_game_key(&g,EC_OK);until_stop(&g);
    assert(g.corner_stop&&fabsf(g.x-.85f)<.001f&&fabsf(g.z+26.8f)<.001f&&fabsf(g.yaw-PI/2)<.001f&&g.score==0);
    ec_game_key(&g,EC_OK);until_stop(&g);
    assert(g.cell==1&&g.score==1&&fabsf(g.x)<.001f&&fabsf(g.z-1.95f)<.001f&&fabsf(g.yaw)<.001f);
    turn(&g,4);ec_game_key(&g,EC_OK);g.anomaly=EC_NORMAL;until_stop(&g);
    assert(g.cell==1&&g.score==1&&fabsf(g.x+.85f)<.001f&&fabsf(g.z-2.8f)<.001f);
    ec_game_key(&g,EC_OK);until_stop(&g);
    assert(g.cell==0&&g.score==0&&g.entry_exit&&fabsf(g.x)<.001f&&fabsf(g.z+25.95f)<.001f);

    /* Eight correct crossings transition to EC_EXITING with continuous traversal */
    ec_game_init(&g,45);ec_game_key(&g,EC_OK);
    for(unsigned i=0;i<8;++i){
        g.anomaly=i%2?EC_RED_LIGHTS:EC_NORMAL;
        bool forward=g.anomaly==EC_NORMAL;
        bool side=forward?!g.entry_exit:g.entry_exit;
        portal(&g,side);
        assert(g.score==i+1);
    }
    assert(g.phase==EC_EXITING&&g.walking&&g.score==8);
    /* OK pauses and unpauses walk without resetting during EXITING */
    ec_game_key(&g,EC_OK);assert(g.phase==EC_EXITING&&!g.walking&&g.score==8);
    ec_game_key(&g,EC_OK);assert(g.phase==EC_EXITING&&g.walking&&g.score==8);
    until_stop(&g); /* Corner assistance rounds into new corridor and stops */
    assert(g.corner_stop&&!g.walking&&g.phase==EC_EXITING&&g.score==8);
    ec_game_key(&g,EC_OK); /* Resume walking towards stairs and terminal doorway */
    until_stop(&g); /* Reaches terminal doorway (north z=-11) */
    assert(g.phase==EC_CLEARED&&!g.walking&&g.score==8);
    assert(fabsf(g.z+11.0f)<.001f);
    assert(ec_game_exit_progress(&g)==1.0f&&fabsf(ec_game_eye_height(&g)-3.15f)<.001f);
    /* Only in EC_CLEARED does OK restart/replay */
    ec_game_key(&g,EC_OK);assert(g.phase==EC_PLAYING&&g.score==0&&g.cell==0);

    for(unsigned a=1;a<=8;++a){g.anomaly=(ec_anomaly_t)a;portal(&g,g.entry_exit);assert(g.score==1);g.anomaly=(ec_anomaly_t)a;portal(&g,!g.entry_exit);assert(g.score==0);}

    /* Every final-round anomaly, entry side and choice: 7->8 transitions to
     * EXITING for correct choices, 7->0 otherwise, with no second judgement on
     * the next movement tick. Evidence describes the departed passage. */
    for(unsigned a=0;a<=8;++a)for(int entry=0;entry<2;++entry)for(int choice=0;choice<2;++choice){
        ec_game_init(&g,123);ec_game_key(&g,EC_OK);g.score=7;
        g.anomaly=(ec_anomaly_t)a;g.entry_exit=entry;portal(&g,choice);
        bool forward=choice!=entry,correct=forward==(a==EC_NORMAL);
        assert(g.score==(correct?8u:0u)&&g.phase==(correct?EC_EXITING:EC_PLAYING));
        assert(g.passages==1&&g.last_judgement.score_before==7&&g.last_judgement.anomaly==(ec_anomaly_t)a);
        assert(g.last_judgement.correct==correct&&g.last_judgement.forward==forward);
        assert(g.last_judgement.entry_exit==entry&&g.last_judgement.crossed_exit==choice);
        ec_game_tick(&g,.1f);
        assert(g.passages==1&&g.score==(correct?8u:0u));
        if(correct) assert(g.phase==EC_EXITING);
    }

    /* Test both exit directions traversal, corner assistance, pause/turns,
     * mid-stairs pause (progress holds without auto-advance), stair top & flat landing
     * retaining EXITING/score8, and terminal doorway clearing (north -11, south -13) */
    for(int dir=0;dir<2;++dir){
        ec_game_init(&g,dir?99:77);ec_game_key(&g,EC_OK);
        /* Reach score 7 */
        for(unsigned i=0;i<7;++i){
            g.anomaly=EC_NORMAL;
            portal(&g,!g.entry_exit);
        }
        assert(g.score==7);
        /* 7 previous normal crossings leave entry_exit=false.
         * For dir=0: forward choice (crossed_exit=true) with EC_NORMAL is correct -> entry_exit becomes false (south connector, goes north).
         * For dir=1: turn-back choice (crossed_exit=false) with anomaly (EC_RED_LIGHTS) is correct -> entry_exit becomes true (north connector, goes south). */
        g.anomaly=dir==0?EC_NORMAL:EC_RED_LIGHTS;
        portal(&g,dir==0?true:false);
        assert(g.score==8&&g.phase==EC_EXITING&&g.walking);
        assert(g.entry_exit==(dir==1));

        /* Portal continuity: position is at connector, yaw preserved */
        if(!g.entry_exit){
            assert(g.x<=-4.5f&&fabsf(g.z-2.8f)<.2f&&fabsf(g.yaw-PI/2)<.01f);
        } else {
            assert(g.x>=4.5f&&fabsf(g.z+26.8f)<.2f&&fabsf(g.yaw+PI/2)<.01f);
        }
        assert(ec_game_exit_progress(&g)==0.0f);
        assert(fabsf(ec_game_eye_height(&g)-1.55f)<.001f);

        /* After cross: tick 6.5s in <=.05s increments reaches auto-corner stop */
        tick_for(&g,6.5f);
        assert(g.corner_stop&&!g.walking&&!g.turning);
        if(!g.entry_exit){
            assert(fabsf(g.x)<.001f&&fabsf(g.z-1.95f)<.001f&&fabsf(g.yaw)<.001f);
        } else {
            assert(fabsf(g.x)<.001f&&fabsf(g.z+25.95f)<.001f&&fabsf(fabsf(g.yaw)-PI)<.001f);
        }

        /* Test pause/turn in exit corridor: disables observation auto alignment */
        ec_game_key(&g,EC_LEFT);
        assert(!g.observing&&!g.walking&&g.score==8&&g.phase==EC_EXITING);
        ec_game_key(&g,EC_RIGHT); /* turn back to hallway heading */

        /* OK walk 3.25s (pre-stair) */
        ec_game_key(&g,EC_OK);assert(g.walking);
        tick_for(&g,3.25f);
        assert(g.walking&&g.phase==EC_EXITING&&g.score==8);
        assert(ec_game_exit_progress(&g)==0.0f); /* Still before stair start */

        /* Pre-stair pause test: stairs must not auto advance while paused */
        ec_game_key(&g,EC_OK);assert(!g.walking);
        float paused_pre_z=g.z,paused_pre_x=g.x;
        for(int t=0;t<20;++t)ec_game_tick(&g,.05f);
        assert(g.z==paused_pre_z&&g.x==paused_pre_x&&g.phase==EC_EXITING);
        ec_game_key(&g,EC_RIGHT);assert(!g.observing);
        ec_game_key(&g,EC_LEFT); /* re-align to hallway heading */

        /* Resume walk until mid-stairs (progress ~ 0.5) */
        ec_game_key(&g,EC_OK);assert(g.walking);
        tick_for(&g,2.25f);
        assert(g.walking&&g.phase==EC_EXITING);
        float mid_p=ec_game_exit_progress(&g);
        assert(mid_p>0.1f&&mid_p<0.9f); /* Confirmed mid-stairs ascent */
        float mid_eye=ec_game_eye_height(&g);
        assert(fabsf(mid_eye-(1.55f+mid_p*1.6f))<.001f);

        /* Paused MID-STAIRS test: stairs must not auto advance while paused mid-ascent */
        ec_game_key(&g,EC_OK);assert(!g.walking);
        float paused_mid_z=g.z;
        for(int t=0;t<20;++t)ec_game_tick(&g,.05f);
        assert(g.z==paused_mid_z&&g.phase==EC_EXITING);
        assert(ec_game_exit_progress(&g)==mid_p);
        assert(ec_game_eye_height(&g)==mid_eye);

        /* Resume walk onto flat landing beyond stair top (-9 for north, -15 for south) */
        ec_game_key(&g,EC_OK);assert(g.walking);
        tick_for(&g,1.5f);
        /* On flat landing: remains EXITING, score 8, walking=true, progress clamped at 1.0, eye 3.15 */
        assert(g.phase==EC_EXITING&&g.walking&&g.score==8);
        assert(ec_game_exit_progress(&g)==1.0f);
        assert(fabsf(ec_game_eye_height(&g)-3.15f)<.001f);
        if(!g.entry_exit){
            assert(g.z<-9.0f&&g.z>-11.0f);
        } else {
            assert(g.z>-15.0f&&g.z<-13.0f);
        }

        /* Continue walking to terminal doorway (-11 for north, -13 for south): clears and stops */
        tick_for(&g,6.0f);
        assert(g.phase==EC_CLEARED&&!g.walking&&g.score==8);
        assert(ec_game_exit_progress(&g)==1.0f);
        assert(fabsf(ec_game_eye_height(&g)-3.15f)<.001f);
        if(!g.entry_exit){
            assert(fabsf(g.z+11.0f)<.001f);
        } else {
            assert(fabsf(g.z+13.0f)<.001f);
        }

        /* Stable 8, no extra judgements or score mutation while cleared */
        unsigned passages_cleared=g.passages;
        for(int t=0;t<20;++t)ec_game_tick(&g,.05f);
        assert(g.phase==EC_CLEARED&&g.score==8&&g.passages==passages_cleared);

        /* Explicit replay only after CLEARED */
        ec_game_key(&g,EC_OK);
        assert(g.phase==EC_PLAYING&&g.score==0&&g.cell==0);
    }

    /* Test exit progress and eye height formula explicitly across both directions:
     * progress clamps at stair top (-9 north, -15 south) and stays 1.0 through landing to doorway */
    {
        ec_game_t eg;
        memset(&eg,0,sizeof(eg));
        eg.phase=EC_EXITING;
        eg.entry_exit=false; /* north: start -5, top -9, door -11 */
        eg.z=0.0f; assert(ec_game_exit_progress(&eg)==0.0f);
        eg.z=-5.0f; assert(ec_game_exit_progress(&eg)==0.0f&&fabsf(ec_game_eye_height(&eg)-1.55f)<.001f);
        eg.z=-7.0f; assert(fabsf(ec_game_exit_progress(&eg)-0.5f)<.001f&&fabsf(ec_game_eye_height(&eg)-2.35f)<.001f);
        eg.z=-9.0f; assert(ec_game_exit_progress(&eg)==1.0f&&fabsf(ec_game_eye_height(&eg)-3.15f)<.001f);
        eg.z=-10.0f; assert(ec_game_exit_progress(&eg)==1.0f&&fabsf(ec_game_eye_height(&eg)-3.15f)<.001f);
        eg.z=-11.0f; assert(ec_game_exit_progress(&eg)==1.0f&&fabsf(ec_game_eye_height(&eg)-3.15f)<.001f);

        eg.entry_exit=true; /* south: start -19, top -15, door -13 */
        eg.z=-22.0f; assert(ec_game_exit_progress(&eg)==0.0f);
        eg.z=-19.0f; assert(ec_game_exit_progress(&eg)==0.0f&&fabsf(ec_game_eye_height(&eg)-1.55f)<.001f);
        eg.z=-17.0f; assert(fabsf(ec_game_exit_progress(&eg)-0.5f)<.001f&&fabsf(ec_game_eye_height(&eg)-2.35f)<.001f);
        eg.z=-15.0f; assert(ec_game_exit_progress(&eg)==1.0f&&fabsf(ec_game_eye_height(&eg)-3.15f)<.001f);
        eg.z=-14.0f; assert(ec_game_exit_progress(&eg)==1.0f&&fabsf(ec_game_eye_height(&eg)-3.15f)<.001f);
        eg.z=-13.0f; assert(ec_game_exit_progress(&eg)==1.0f&&fabsf(ec_game_eye_height(&eg)-3.15f)<.001f);

        eg.phase=EC_PLAYING;
        assert(ec_game_exit_progress(&eg)==0.0f&&fabsf(ec_game_eye_height(&eg)-1.55f)<.001f);
    }

    /* Test boundary restriction: exit corridor prevents moving back into loop or scoring */
    {
        ec_game_t eg;
        memset(&eg,0,sizeof(eg));
        eg.phase=EC_EXITING;eg.score=8;eg.walking=true;
        /* South connector boundary: x cannot go < -4.8 */
        eg.entry_exit=false;eg.x=-4.75f;eg.z=2.8f;eg.yaw=-PI/2; /* facing west */
        for(int i=0;i<20;++i)ec_game_tick(&eg,.05f);
        assert(eg.x>=-4.8f&&eg.score==8&&eg.phase==EC_EXITING&&eg.passages==0);

        /* North connector boundary: x cannot go > 4.8 */
        eg.entry_exit=true;eg.x=4.75f;eg.z=-26.8f;eg.yaw=PI/2; /* facing east */
        for(int i=0;i<20;++i)ec_game_tick(&eg,.05f);
        assert(eg.x<=4.8f&&eg.score==8&&eg.phase==EC_EXITING&&eg.passages==0);
    }

    /* Manual takeover during turning: all 4 corners x 9 fractions x 2 keys [L/R]
     * snaps target heading to nearest 45-deg grid then applies requested +/-45 step.
     * Asserts shortest delta <= 22.5 deg, grid target, unchanged position and camera_yaw
     * immediately, walking stopped, assistance cancelled, no passage change, and continued
     * eight-key grid stability. All loops strictly bounded by maximum iteration count. */
    {
        const float test_fracs[] = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f};
        const float init_x[] = {0.0f, 0.0f, -1.3f, 1.3f};
        const float init_z[] = {-25.3f, 1.3f, 2.8f, -26.8f};
        const float init_yaw[] = {0.0f, PI, PI / 2.0f, -PI / 2.0f};

        for (int c = 1; c <= 4; ++c) {
            for (int fi = 0; fi < 9; ++fi) {
                for (int k = 0; k < 2; ++k) {
                    ec_game_t eg;
                    memset(&eg, 0, sizeof(eg));
                    eg.rng = 12345;
                    eg.phase = EC_PLAYING;
                    eg.x = init_x[c - 1]; eg.z = init_z[c - 1];
                    eg.yaw = eg.camera_yaw = init_yaw[c - 1];
                    eg.walking = true;

                    for (int step = 0; step < 200 && !eg.turning && eg.walking; ++step) ec_game_tick(&eg, 0.025f);
                    assert(eg.turning && eg.corner_id == c);
                    float target_t = test_fracs[fi] * 1.15f;
                    for (int step = 0; step < 200 && eg.turning && eg.turn_elapsed < target_t; ++step) ec_game_tick(&eg, 0.025f);
                    assert(eg.turning);

                    float mid_x = eg.x, mid_z = eg.z, mid_yaw = eg.yaw, mid_cam = eg.camera_yaw;
                    unsigned passages_before = eg.passages;

                    /* Shortest delta to 45-deg grid */
                    float expected_base = roundf(atan2f(sinf(mid_yaw), cosf(mid_yaw)) / (PI / 4.0f)) * (PI / 4.0f);
                    float shortest_delta = fabsf(atan2f(sinf(mid_yaw - expected_base), cosf(mid_yaw - expected_base)));
                    assert(shortest_delta <= (PI / 8.0f) + 1e-4f);

                    ec_key_t key = (k == 0) ? EC_LEFT : EC_RIGHT;
                    float step = (k == 0) ? -PI / 4.0f : PI / 4.0f;
                    float expected_new_yaw = atan2f(sinf(expected_base + step), cosf(expected_base + step));

                    ec_game_key(&eg, key);

                    /* Unchanged position and camera immediately */
                    assert(eg.x == mid_x && eg.z == mid_z);
                    assert(eg.camera_yaw == mid_cam);
                    assert(!eg.walking && !eg.turning);
                    assert(eg.corner_id == 0 && eg.bypass_id == 0 && !eg.corner_stop);
                    assert(eg.passages == passages_before);

                    /* Correct direction and grid target */
                    float err_target = fabsf(atan2f(sinf(eg.yaw - expected_new_yaw), cosf(eg.yaw - expected_new_yaw)));
                    assert(err_target < 1e-4f);

                    /* 8-way 45-degree grid alignment */
                    float norm = atan2f(sinf(eg.yaw), cosf(eg.yaw));
                    float grid_diff = fabsf(atan2f(sinf(norm - roundf(norm / (PI / 4.0f)) * (PI / 4.0f)),
                                                   cosf(norm - roundf(norm / (PI / 4.0f)) * (PI / 4.0f))));
                    assert(grid_diff < 1e-4f);

                    /* Continued eight-key grid stability */
                    float snap_start = eg.yaw;
                    for (int step_idx = 1; step_idx <= 8; ++step_idx) {
                        ec_game_key(&eg, EC_RIGHT);
                        float n = atan2f(sinf(eg.yaw), cosf(eg.yaw));
                        float gd = fabsf(atan2f(sinf(n - roundf(n / (PI / 4.0f)) * (PI / 4.0f)),
                                                cosf(n - roundf(n / (PI / 4.0f)) * (PI / 4.0f))));
                        assert(gd < 1e-4f);
                    }
                    assert(fabsf(atan2f(sinf(eg.yaw - snap_start), cosf(eg.yaw - snap_start))) < 1e-4f);
                }
            }
        }

        /* Paused and resumed takeover tests */
        for (int c = 1; c <= 4; ++c) {
            /* 1. Takeover while paused in corner */
            ec_game_t eg;
            memset(&eg, 0, sizeof(eg));
            eg.rng = 12345; eg.phase = EC_PLAYING;
            eg.x = init_x[c - 1]; eg.z = init_z[c - 1];
            eg.yaw = eg.camera_yaw = init_yaw[c - 1]; eg.walking = true;
            for (int step = 0; step < 200 && !eg.turning && eg.walking; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.turning);
            for (int step = 0; step < 200 && eg.turning && eg.turn_elapsed < 0.4f; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.turning);

            ec_game_key(&eg, EC_OK); /* pause */
            assert(!eg.walking && eg.turning);
            float px = eg.x, pz = eg.z, pcam = eg.camera_yaw, pyaw = eg.yaw;
            ec_game_key(&eg, EC_RIGHT); /* takeover while paused */
            assert(!eg.walking && !eg.turning);
            assert(eg.x == px && eg.z == pz && eg.camera_yaw == pcam);
            assert(eg.corner_id == 0 && eg.bypass_id == 0 && !eg.corner_stop);
            float base = roundf(atan2f(sinf(pyaw), cosf(pyaw)) / (PI / 4.0f)) * (PI / 4.0f);
            assert(fabsf(atan2f(sinf(eg.yaw - (base + PI / 4.0f)), cosf(eg.yaw - (base + PI / 4.0f)))) < 1e-4f);

            /* 2. Takeover after resume in corner */
            memset(&eg, 0, sizeof(eg));
            eg.rng = 12345; eg.phase = EC_PLAYING;
            eg.x = init_x[c - 1]; eg.z = init_z[c - 1];
            eg.yaw = eg.camera_yaw = init_yaw[c - 1]; eg.walking = true;
            for (int step = 0; step < 200 && !eg.turning && eg.walking; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.turning);
            for (int step = 0; step < 200 && eg.turning && eg.turn_elapsed < 0.3f; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.turning);
            ec_game_key(&eg, EC_OK); /* pause */
            for (int t = 0; t < 10; ++t) ec_game_tick(&eg, 0.05f);
            ec_game_key(&eg, EC_OK); /* resume */
            assert(eg.walking && eg.turning);
            ec_game_tick(&eg, 0.05f);
            float rx = eg.x, rz = eg.z, rcam = eg.camera_yaw, ryaw = eg.yaw;
            ec_game_key(&eg, EC_LEFT); /* takeover while resumed */
            assert(!eg.walking && !eg.turning);
            assert(eg.x == rx && eg.z == rz && eg.camera_yaw == rcam);
            assert(eg.corner_id == 0 && eg.bypass_id == 0 && !eg.corner_stop);
            base = roundf(atan2f(sinf(ryaw), cosf(ryaw)) / (PI / 4.0f)) * (PI / 4.0f);
            assert(fabsf(atan2f(sinf(eg.yaw - (base - PI / 4.0f)), cosf(eg.yaw - (base - PI / 4.0f)))) < 1e-4f);
        }

        /* Actual-position recovery: cancel arc, re-orient toward corner, resume walking without position reset,
         * verify bypass_id does not suppress assistance, and corner assistance triggers cleanly */
        for (int c = 1; c <= 4; ++c) {
            ec_game_t eg;
            memset(&eg, 0, sizeof(eg));
            eg.rng = 12345; eg.phase = EC_PLAYING; eg.anomaly = EC_NORMAL;
            eg.x = init_x[c - 1]; eg.z = init_z[c - 1];
            eg.yaw = eg.camera_yaw = init_yaw[c - 1]; eg.walking = true;

            for (int step = 0; step < 200 && !eg.turning && eg.walking; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.turning && eg.corner_id == c);
            for (int step = 0; step < 200 && eg.turning && eg.turn_elapsed < 0.25f; ++step) ec_game_tick(&eg, 0.025f);

            /* Cancel arc: press RIGHT then LEFT to restore approach heading */
            ec_game_key(&eg, EC_RIGHT);
            assert(!eg.turning && !eg.walking && eg.bypass_id == 0);
            ec_game_key(&eg, EC_LEFT);
            assert(fabsf(atan2f(sinf(eg.yaw - init_yaw[c - 1]), cosf(eg.yaw - init_yaw[c - 1]))) < 1e-4f);

            /* Resume walking from actual unchanged position */
            ec_game_key(&eg, EC_OK);
            assert(eg.walking && eg.bypass_id == 0);

            /* Corner assistance must re-trigger (not suppressed by bypass_id) */
            for (int step = 0; step < 200 && eg.walking && !eg.turning; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.turning && eg.corner_id == c);

            /* Complete the corner cleanly to corner_stop */
            for (int step = 0; step < 200 && eg.turning && eg.walking; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.corner_stop && !eg.turning && !eg.walking);
        }

        /* Subsequent observation and corner assistance actually recover after full 180 turnaround */
        {
            ec_game_t eg;
            memset(&eg, 0, sizeof(eg));
            eg.rng = 12345; eg.phase = EC_PLAYING; eg.anomaly = EC_NORMAL;
            eg.x = 0; eg.z = -25.3f; eg.yaw = eg.camera_yaw = 0; eg.walking = true;

            for (int step = 0; step < 200 && !eg.turning && eg.walking; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.turning && eg.corner_id == 1);
            for (int step = 0; step < 200 && eg.turning && eg.turn_elapsed < 0.35f; ++step) ec_game_tick(&eg, 0.025f);

            /* Takeover */
            ec_game_key(&eg, EC_LEFT);
            /* Turn 180 deg to South (yaw = PI): 3 more LEFT turns */
            for (int i = 0; i < 3; ++i) ec_game_key(&eg, EC_LEFT);
            assert(fabsf(atan2f(sinf(eg.yaw - PI), cosf(eg.yaw - PI))) < 1e-4f);

            /* Walk south */
            ec_game_key(&eg, EC_OK);
            assert(eg.walking);
            /* Walk to fixture anchor at z = -8.0 */
            for (int step = 0; step < 500 && eg.walking && eg.z < -8.2f; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.z >= -8.2f);

            /* Turn to face East (yaw = PI/2): 2 left turns */
            ec_game_key(&eg, EC_LEFT);
            ec_game_key(&eg, EC_LEFT);
            assert(eg.observing); /* Observation successfully triggers! */
            for (int step = 0; step < 100 && eg.observing; ++step) ec_game_tick(&eg, 0.025f);
            assert(!eg.observing && fabsf(eg.z - (-8.0f)) < 0.001f);

            /* Turn back to South: 2 right turns */
            ec_game_key(&eg, EC_RIGHT);
            ec_game_key(&eg, EC_RIGHT);
            assert(fabsf(atan2f(sinf(eg.yaw - PI), cosf(eg.yaw - PI))) < 1e-4f);

            /* Resume walking south towards Corner 2 */
            ec_game_key(&eg, EC_OK);
            for (int step = 0; step < 500 && eg.walking && !eg.turning; ++step) ec_game_tick(&eg, 0.025f);

            /* Corner 2 successfully detected and triggered! */
            assert(eg.turning && eg.corner_id == 2);
            for (int step = 0; step < 200 && eg.turning && eg.walking; ++step) ec_game_tick(&eg, 0.025f);
            assert(eg.corner_stop && !eg.turning && !eg.walking);
            assert(fabsf(eg.x - (-0.85f)) < 0.001f && fabsf(eg.z - 2.8f) < 0.001f);
        }
    }

    ec_game_init(&g,123);ec_game_key(&g,EC_OK);
    g.x=0;g.z=-26.6f;g.yaw=0;g.walking=true;g.bypass_id=0;
    for(int i=0;i<10&&g.walking;++i)ec_game_tick(&g,50);
    assert(g.corner_stop&&fabsf(g.z+26.8f)<.001f&&fabsf(g.x-.85f)<.001f);
    ec_game_key(&g,EC_OK);ec_game_tick(&g,.05f);assert(g.walking);
    assert(!ec_game_walkable(1.59f,-10)&&ec_game_walkable(0,-10));
    g.anomaly=EC_NORMAL;g.walking=false;
    for(int i=0;i<3600;++i){float x=g.npc_x,z=g.npc_z;ec_game_tick(&g,1/60.0f);assert(fabsf(g.npc_x-x)<.001f&&fabsf(g.npc_z-z)<.02f);}
    /* Four rounded corners, displaced approaches, bounded motion, and pause.
     * No automatic arc crosses a scoring boundary or decides an anomaly. */
    for(int corner=0;corner<4;++corner)for(int offset=-1;offset<=1;++offset){
        ec_game_init(&g,123);ec_game_key(&g,EC_OK);
        const float x[]={offset*.5f,offset*.5f,-1.3f,1.3f};
        const float z[]={-25.3f,1.3f,2.8f+offset*.5f,-26.8f+offset*.5f};
        const float yaw[]={0,PI,PI/2,-PI/2};
        const float end_x[]={.85f,-.85f,0,0},end_z[]={-26.8f,2.8f,1.95f,-25.95f};
        const float end_yaw[]={PI/2,-PI/2,0,PI};
        g.x=x[corner];g.z=z[corner];g.yaw=g.camera_yaw=yaw[corner];g.walking=true;
        bool paused=false,seen_turn=false;
        for(int i=0;i<1000&&g.walking;++i){
            float px=g.x,pz=g.z;ec_game_tick(&g,.025f);
            assert(hypotf(g.x-px,g.z-pz)<.12f&&ec_game_walkable(g.x,g.z));
            if(g.turning){seen_turn=true;
                if(!paused&&g.turn_elapsed>.3f){
                    ec_game_key(&g,EC_OK);float tx=g.x,tz=g.z,elapsed=g.turn_elapsed;
                    ec_game_tick(&g,.2f);assert(g.x==tx&&g.z==tz&&g.turn_elapsed==elapsed&&!g.walking);
                    ec_game_key(&g,EC_OK);paused=true;
                }
            }
        }
        assert(seen_turn&&paused&&g.corner_stop&&!g.walking&&!g.turning&&g.passages==0);
        assert(fabsf(g.x-end_x[corner])<.001f&&fabsf(g.z-end_z[corner])<.001f);
        assert(cosf(g.yaw-end_yaw[corner])>.9999f);
    }
    ec_game_init(&g,123);ec_game_key(&g,EC_OK);g.z=-26;g.walking=true;
    ec_game_tick(&g,.1f);assert(g.turning);ec_game_key(&g,EC_LEFT);
    assert(!g.turning&&!g.walking&&g.passages==0);
    /* Observation assistance follows nominal anchors in every anomaly; no
     * jump, forced wall approach, portal judgement or hidden answer cue. */
    unsigned observation_cases=0;
    for(unsigned a=0;a<=8;++a)for(int side=0;side<2;++side)for(int k=0;k<(side?5:4);++k)for(int offset=-1;offset<=1;offset+=2){
        const float left[]={-3.5f,-5.8f,-8,-18},right[]={-5,-8,-11,-16,-20};
        float target=side?right[k]:left[k];
        ec_game_init(&g,123);ec_game_key(&g,EC_OK);g.anomaly=(ec_anomaly_t)a;
        g.x=side?.4f:-.4f;g.z=target+offset*.6f;g.walking=true;float start=g.z;
        ec_key_t key=side?EC_RIGHT:EC_LEFT;
        ec_game_key(&g,key);assert(!g.walking&&!g.observing&&g.z==start);
        ec_game_key(&g,key);assert(g.observing&&g.z==start);
        for(int n=0;n<40;++n){float previous=g.z;ec_game_tick(&g,.02f);
            assert(fabsf(g.z-previous)<.031f&&fabsf(g.z-target)<=fabsf(previous-target)+.00001f);
            assert(ec_game_walkable(g.x,g.z)&&g.x==(side?.4f:-.4f));
        }
        assert(!g.observing&&!g.walking&&fabsf(g.z-target)<.00001f&&g.passages==0&&g.score==0);
        ++observation_cases;
    }
    ec_game_init(&g,1);ec_game_key(&g,EC_OK);g.z=-4.4f;turn(&g,2);assert(g.observing);
    ec_game_tick(&g,.2f);float stopped=g.z;ec_game_key(&g,EC_LEFT);assert(!g.observing);
    ec_game_tick(&g,.2f);assert(g.z==stopped);
    ec_game_key(&g,EC_RIGHT);assert(g.observing);ec_game_key(&g,EC_OK);assert(!g.observing&&g.walking);
    ec_game_init(&g,1);ec_game_key(&g,EC_OK);g.z=-3.7f;turn(&g,2);assert(!g.observing);
    ec_game_init(&g,1);ec_game_key(&g,EC_OK);g.z=-4.5f;g.x=1.1f;turn(&g,2);assert(!g.observing);
    /* The user's visible 45-degree poster at z=-19.131 must be caught on
     * the next turn, despite being outside the original 0.65 m radius. */
    for(unsigned a=0;a<=8;++a)for(int wall=0;wall<2;++wall)for(int north=0;north<2;++north){
        ec_game_init(&g,1);ec_game_key(&g,EC_OK);g.anomaly=(ec_anomaly_t)a;
        float target=wall?-20:-18,start=target+(north?.869f:-.869f);
        g.z=start;g.yaw=g.camera_yaw=(wall?1:-1)*(north?PI/4:PI*.75f);
        ec_game_key(&g,wall==north?EC_RIGHT:EC_LEFT);
        assert(g.observing&&g.z==start);
        for(int n=0;n<80;++n){float old=g.z;ec_game_tick(&g,.02f);assert(fabsf(g.z-old)<=.0241f);}
        assert(fabsf(g.z-target)<.0001f&&!g.observing&&g.x==0&&g.passages==0);
    }
    ec_game_init(&g,1);ec_game_key(&g,EC_OK);g.z=-19.131f;g.yaw=g.camera_yaw=PI*.75f;
    ec_game_key(&g,EC_LEFT);assert(!g.observing); /* Far fixture was behind the diagonal view. */
    ec_game_init(&g,1);ec_game_key(&g,EC_OK);g.z=-18.7f;g.yaw=PI/4;
    ec_game_key(&g,EC_RIGHT);assert(!g.observing); /* Visible but exceeds travel cap. */
    printf("Corridor observation: %u anchor/anomaly/direction cases plus 36 visible-diagonal cases and cancellation/range guards PASS\n",observation_cases);
    puts("Corridor: traversal, eight anomalies, entry-relative scoring, corners, replay, collision and 3600 NPC frames PASS");
}
