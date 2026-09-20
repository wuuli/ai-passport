#include "corridor_game.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846f
static float clampf(float x, float a, float b) { return fmaxf(a, fminf(b, x)); }
static float angle(float a) { return atan2f(sinf(a), cosf(a)); }
static uint32_t random_next(ec_game_t *g) {
    uint32_t x = g->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return g->rng = x;
}
static const float walls[6][4] = {
    {-1.6f,-28.4f,-1.6f,1.2f}, {1.6f,-25.2f,1.6f,4.4f},
    {-1.6f,-28.4f,4.8f,-28.4f}, {1.6f,-25.2f,4.8f,-25.2f},
    {-4.8f,1.2f,-1.6f,1.2f}, {-4.8f,4.4f,1.6f,4.4f}
};
bool ec_game_walkable(float x, float z) {
    bool in = false;
    for (int i=-1; i<=1; ++i) {
        float xx=x-i*9.6f, zz=z+i*29.6f;
        in |= (xx>=-1.6f && xx<=1.6f && zz>=-28.4f && zz<=4.4f) ||
              (xx>=1.6f && xx<=4.8f && zz>=-28.4f && zz<=-25.2f) ||
              (xx>=-4.8f && xx<=-1.6f && zz>=1.2f && zz<=4.4f);
        for (unsigned w=0; w<6; ++w) {
            float dx=xx-clampf(xx,walls[w][0],walls[w][2]);
            float dz=zz-clampf(zz,walls[w][1],walls[w][3]);
            if (dx*dx+dz*dz < .03999f) return false;
        }
    }
    return in;
}
static void npc_reset(ec_game_t *g) {
    g->npc_x=.768f; g->npc_z=-21.6f; g->npc_dir=1;
    g->npc_heading=0; g->npc_distance=0; g->npc_turn=0;
}
void ec_game_init(ec_game_t *g, uint32_t seed) {
    memset(g,0,sizeof(*g)); g->rng=seed ? seed : 0x45c013u;
    g->phase=EC_TITLE; g->z=-1.8f; npc_reset(g);
}
/* Fixed nominal fixture positions: never inspect anomaly state when choosing
 * a target, including a door that is absent or an extra vent. */
static void observe_begin(ec_game_t *g,float previous_yaw) {
    static const float left[]={-3.5f,-5.8f,-8,-18};
    static const float right[]={-5,-8,-11,-16,-20};
    bool east=fabsf(angle(g->yaw-PI/2))<.01f;
    bool west=fabsf(angle(g->yaw+PI/2))<.01f;
    if((!east&&!west)||fabsf(g->x)>1.2f||g->z> -2||g->z< -23) return;
    if((east?1.6f-g->x:1.6f+g->x)<.65f) return;
    const float *anchors=east?right:left;
    unsigned count=east?sizeof(right)/sizeof(*right):sizeof(left)/sizeof(*left);
    float nearest=1.20001f,target=g->z;
    float fx=sinf(previous_yaw),fz=-cosf(previous_yaw);
    float dx=(east?1.6f:-1.6f)-g->x;
    for(unsigned i=0;i<count;++i){
        float dz=anchors[i]-g->z,d=fabsf(dz);
        float depth=dx*fx+dz*fz,lateral=dx*(-fz)+dz*fx;
        /* Match the native camera's 0.6 half-frustum. Nominal fixture width
         * admits a partly visible edge; missing/changed fixtures use the
         * same geometry, so anomaly state never reveals the answer. */
        float edge=.425f*(fabsf(fx)+.6f*fabsf(fz));
        bool visible=depth>.35f&&fabsf(lateral)<=depth*.6f+edge;
        if(d<nearest&&(d<=.65f||visible)){nearest=d;target=anchors[i];}
    }
    if(nearest>1.2f||nearest<.005f) return;
    g->observing=true;g->observe_elapsed=0;
    g->observe_start_z=g->z;g->observe_target_z=target;
    g->observe_seconds=fmaxf(.65f,nearest*1.25f); /* Smoothstep peak <=1.2 m/s. */
}
static void observe_step(ec_game_t *g,float seconds) {
    g->observe_elapsed=fminf(g->observe_seconds,g->observe_elapsed+seconds);
    float t=g->observe_elapsed/g->observe_seconds,s=t*t*(3-2*t);
    float z=g->observe_start_z+(g->observe_target_z-g->observe_start_z)*s;
    if(ec_game_walkable(g->x,z))g->z=z;
    else g->observing=false;
    if(g->observe_elapsed>=g->observe_seconds)g->observing=false;
}
void ec_game_key(ec_game_t *g, ec_key_t key) {
    g->observing=false; /* Every physical input overrides the current alignment. */
    if (key==EC_OK) {
        if (g->phase!=EC_PLAYING) {
            uint32_t seed=g->rng; ec_game_init(g,seed); g->phase=EC_PLAYING;
        } else { g->walking=!g->walking; }
        if (g->walking) { g->bypass_id=g->corner_id; g->corner_stop=false; }
    } else if (g->phase==EC_PLAYING) {
        g->turning=false; /* Manual observation/turn-back always overrides assistance. */
        float previous_yaw=g->yaw;
        g->yaw=angle(g->yaw+(key==EC_RIGHT ? PI/4 : -PI/4));
        g->walking=false;
        observe_begin(g,previous_yaw);
    }
}
static void cross(ec_game_t *g, bool exit_side) {
    bool forward = exit_side != g->entry_exit;
    bool correct = forward == (g->anomaly==EC_NORMAL);
    g->last_judgement=(ec_judgement_t){g->score,g->anomaly,g->entry_exit,exit_side,forward,correct};
    g->score=correct ? g->score+1 : 0;
    ++g->passages;
    if (exit_side) { g->x-=9.6f; g->z+=29.6f; ++g->cell; }
    else { g->x+=9.6f; g->z-=29.6f; --g->cell; }
    g->entry_exit=!exit_side;
    g->corner_id=0; g->bypass_id=0; g->corner_stop=false;g->turning=false;g->observing=false;
    if (g->score>=8) { g->score=8; g->phase=EC_CLEARED; g->walking=false; return; }
    ec_anomaly_t old=g->anomaly;
    if ((random_next(g)&1u)==0) g->anomaly=EC_NORMAL;
    else {
        g->anomaly=(ec_anomaly_t)(1+random_next(g)%8);
        if (g->anomaly==old) g->anomaly=(ec_anomaly_t)(1+(unsigned)g->anomaly%8);
    }
    npc_reset(g);
}
static int corner(const ec_game_t *g,float *distance) {
    if (fabsf(angle(g->yaw))<.25f && fabsf(g->x)<1.4f && g->z>=-26.8f) {
        *distance=g->z+26.8f; return 1;
    }
    if (fabsf(angle(g->yaw-PI))<.25f && fabsf(g->x)<1.4f && g->z<=2.8f) {
        *distance=2.8f-g->z; return 2;
    }
    if (fabsf(angle(g->yaw-PI/2))<.25f && g->z>1.4f && g->x<=0) {
        *distance=-g->x; return 3;
    }
    if (fabsf(angle(g->yaw+PI/2))<.25f && g->z<-25.4f && g->x>=0) {
        *distance=g->x; return 4;
    }
    *distance=100; return 0;
}
/* Rounded centre-line corners stop after the camera can see the next leg.
 * They never reach a judgement portal (x=+/-4.8). OK pauses/resumes the arc;
 * a direction key cancels assistance so the player may deliberately return. */
#define CORNER_RADIUS .85f
#define CORNER_SECONDS 1.15f
static void turn_step(ec_game_t *g,float seconds) {
    g->turn_elapsed=fminf(CORNER_SECONDS,g->turn_elapsed+seconds);
    float t=g->turn_elapsed/CORNER_SECONDS,s=t*t*(3-2*t),a=s*PI/2;
    float c=cosf(a),sn=sinf(a),x,z,start_x,start_z,yaw;
    switch(g->corner_id){
    case 1: start_x=0;start_z=-26.8f+CORNER_RADIUS;
        x=CORNER_RADIUS*(1-c);z=-26.8f+CORNER_RADIUS*(1-sn);yaw=a;break;
    case 2: start_x=0;start_z=2.8f-CORNER_RADIUS;
        x=-CORNER_RADIUS*(1-c);z=2.8f-CORNER_RADIUS*(1-sn);yaw=PI+a;break;
    case 3: start_x=-CORNER_RADIUS;start_z=2.8f;
        x=-CORNER_RADIUS*(1-sn);z=2.8f-CORNER_RADIUS*(1-c);yaw=PI/2-a;break;
    default: start_x=CORNER_RADIUS;start_z=-26.8f;
        x=CORNER_RADIUS*(1-sn);z=-26.8f+CORNER_RADIUS*(1-c);yaw=-PI/2-a;break;
    }
    /* Approach offsets decay continuously; never snap a displaced player. */
    float remaining=(1-s)*(1-s);
    x+=(g->turn_start_x-start_x)*remaining;z+=(g->turn_start_z-start_z)*remaining;
    if(!ec_game_walkable(x,z)){g->walking=false;g->turning=false;return;}
    g->x=x;g->z=z;g->yaw=angle(yaw);
    if(g->turn_elapsed>=CORNER_SECONDS){
        g->turning=false;g->walking=false;g->corner_stop=true;
        g->bypass_id=g->corner_id;
    }
}
static void move_step(ec_game_t *g,float seconds) {
    if(g->turning){turn_step(g,seconds);return;}
    float dist; int id=corner(g,&dist);
    float approach=dist-CORNER_RADIUS,speed=1.62f;
    bool assist=id && id!=g->bypass_id;
    if(assist&&approach<.8f)speed*=fmaxf(.28f,approach/.8f);
    float travel=speed*seconds;
    if(assist&&travel>=approach){
        /* Start at the actual position to preserve continuity even when the
         * player resumes inside the approach zone after manual observation. */
        g->corner_id=id;g->turning=true;g->turn_elapsed=0;
        g->turn_start_x=g->x;g->turn_start_z=g->z;g->corner_stop=false;
        turn_step(g,seconds);return;
    }
    float nx=g->x+sinf(g->yaw)*travel,nz=g->z-cosf(g->yaw)*travel;
    if(ec_game_walkable(nx,g->z))g->x=nx;
    if(ec_game_walkable(g->x,nz))g->z=nz;
    if(g->x>4.8f&&g->z>=-28.2f&&g->z<=-25.4f)cross(g,true);
    else if(g->x< -4.8f&&g->z>=1.4f&&g->z<=4.2f)cross(g,false);
}
void ec_game_tick(ec_game_t *g,float seconds) {
    if(g->phase!=EC_PLAYING || !isfinite(seconds) || seconds<=0) return;
    seconds=fminf(seconds,.25f);
    if(g->observing)observe_step(g,seconds);
    float remaining=seconds;
    while(remaining>0 && g->walking) {
        float step=fminf(remaining,.04f); move_step(g,step); remaining-=step;
    }
    float blend=1-expf(-14*seconds);
    g->camera_yaw=angle(g->camera_yaw+angle(g->yaw-g->camera_yaw)*blend);
    if(g->anomaly==EC_STARING_NPC) {
        float target=atan2f(g->x-g->npc_x,g->z-g->npc_z);
        g->npc_heading=angle(g->npc_heading+angle(target-g->npc_heading)*blend);
        return;
    }
    if(g->npc_turn>0) g->npc_turn=fmaxf(0,g->npc_turn-seconds);
    else {
        float travel=.85f*seconds;
        float target=clampf(g->npc_z+g->npc_dir*travel,-22,-2);
        g->npc_distance+=fabsf(target-g->npc_z); g->npc_z=target;
        if(target<=-22 || target>=-2) { g->npc_dir=-g->npc_dir; g->npc_turn=.6f; }
    }
    float heading=g->npc_dir>0 ? 0 : PI;
    g->npc_heading=angle(g->npc_heading+angle(heading-g->npc_heading)*(1-expf(-6*seconds)));
}
