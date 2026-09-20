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
int main(void){
    ec_game_t g;ec_game_init(&g,123);assert(g.phase==EC_TITLE&&!g.walking);
    ec_game_key(&g,EC_OK);assert(g.phase==EC_PLAYING&&g.anomaly==EC_NORMAL&&!g.walking);
    ec_game_key(&g,EC_OK);until_stop(&g);
    assert(g.corner_stop&&fabsf(g.x-.85f)<.001f&&fabsf(g.z+26.8f)<.001f&&fabsf(g.yaw-PI/2)<.001f&&g.score==0);
    ec_game_key(&g,EC_OK);until_stop(&g);
    assert(g.cell==1&&g.score==1&&fabsf(g.x)<.001f&&fabsf(g.z-1.95f)<.001f&&fabsf(g.yaw)<.001f);
    turn(&g,4);ec_game_key(&g,EC_OK);g.anomaly=EC_NORMAL;until_stop(&g);
    assert(g.cell==1&&g.score==1&&fabsf(g.x+.85f)<.001f&&fabsf(g.z-2.8f)<.001f);
    ec_game_key(&g,EC_OK);until_stop(&g);
    assert(g.cell==0&&g.score==0&&g.entry_exit&&fabsf(g.x)<.001f&&fabsf(g.z+25.95f)<.001f);
    ec_game_init(&g,45);ec_game_key(&g,EC_OK);
    for(unsigned i=0;i<8;++i){g.anomaly=i%2?EC_RED_LIGHTS:EC_NORMAL;bool forward=g.anomaly==EC_NORMAL;bool side=forward?!g.entry_exit:g.entry_exit;portal(&g,side);assert(g.score==i+1);}
    assert(g.phase==EC_CLEARED&&!g.walking);ec_game_key(&g,EC_OK);assert(g.phase==EC_PLAYING&&g.score==0&&g.cell==0);
    for(unsigned a=1;a<=8;++a){g.anomaly=(ec_anomaly_t)a;portal(&g,g.entry_exit);assert(g.score==1);g.anomaly=(ec_anomaly_t)a;portal(&g,!g.entry_exit);assert(g.score==0);}
    /* Every final-round anomaly, entry side and choice: 7->8 only for
     * correct choices, 7->0 otherwise, with no second judgement on the
     * next movement tick. Evidence must describe the departed passage. */
    for(unsigned a=0;a<=8;++a)for(int entry=0;entry<2;++entry)for(int choice=0;choice<2;++choice){
        ec_game_init(&g,123);ec_game_key(&g,EC_OK);g.score=7;
        g.anomaly=(ec_anomaly_t)a;g.entry_exit=entry;portal(&g,choice);
        bool forward=choice!=entry,correct=forward==(a==EC_NORMAL);
        assert(g.score==(correct?8u:0u)&&g.phase==(correct?EC_CLEARED:EC_PLAYING));
        assert(g.passages==1&&g.last_judgement.score_before==7&&g.last_judgement.anomaly==(ec_anomaly_t)a);
        assert(g.last_judgement.correct==correct&&g.last_judgement.forward==forward);
        assert(g.last_judgement.entry_exit==entry&&g.last_judgement.crossed_exit==choice);
        ec_game_tick(&g,.1f);assert(g.passages==1);
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
