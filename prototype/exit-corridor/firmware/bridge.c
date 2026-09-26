/* Browser boundary only. Rules and pixels are the unmodified firmware C core. */
#include "corridor_game.h"
#include "corridor_render.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef EC_SPRITE_BYTES
#define EC_SPRITE_BYTES 312689
#endif
static uint8_t sprite[EC_SPRITE_BYTES], image[EC_IMAGE_BYTES];
static ec_game_t game;
static ec_renderer_t *renderer;
uint8_t *web_sprite(void) { return sprite; }
unsigned web_sprite_size(void) { return sizeof(sprite); }
int web_init(uint32_t seed) {
    if (!renderer) renderer=ec_renderer_create(sprite,sizeof(sprite));
    ec_game_init(&game,seed);
    return renderer != NULL;
}
void web_key(int key) { if (key>=EC_LEFT && key<=EC_OK) ec_game_key(&game,(ec_key_t)key); }
void web_tick(float seconds) { ec_game_tick(&game,seconds); }
void web_pause(void) { game.walking=false;game.observing=false; }
void web_title(void) { ec_game_init(&game,game.rng); }
uint8_t *web_draw(void) { if(renderer)ec_renderer_draw(renderer,&game,image);return image; }
void web_destroy(void) { ec_renderer_destroy(renderer);renderer=NULL; }
/* Stable named field order is checked by the native/Wasm parity tests. */
double web_state(unsigned field) {
    switch(field) {
    case 0:return game.phase;case 1:return game.anomaly;
    case 2:return game.x;case 3:return game.z;case 4:return game.yaw;case 5:return game.camera_yaw;
    case 6:return game.walking;case 7:return game.turning;case 8:return game.observing;
    case 9:return game.score;case 10:return game.passages;case 11:return game.cell;case 12:return game.entry_exit;
    case 13:return game.npc_x;case 14:return game.npc_z;case 15:return game.npc_heading;
    case 16:return game.npc_distance;case 17:return game.npc_turn;case 18:return game.rng;
    case 19:return game.last_judgement.score_before;case 20:return game.last_judgement.anomaly;
    case 21:return game.last_judgement.correct;case 22:return game.last_judgement.forward;
    case 23:return game.corner_stop;case 24:return game.corner_id;
    case 25:return game.hud_score;case 26:return game.hud_score_pending;default:return 0;
    }
}
/* Explicit review fixtures only; normal play never calls this entry point. */
int web_review(int anomaly,float x,float z,float yaw,unsigned score,int entry) {
    if(anomaly<EC_NORMAL||anomaly>EC_ABSENT_NPC||score>8||!isfinite(yaw)||
       !isfinite(x)||!isfinite(z)||fabsf(x)>4.8f||z< -28.2f||z>4.2f||!ec_game_walkable(x,z))return 0;
    if(score==8&&((entry?z>=-13:z<=-11)||fabsf(x)>1.4f))return 0;
    ec_game_init(&game,12345);
    if(renderer)ec_renderer_draw(renderer,&game,image); /* Clear renderer history before replacing a review cell. */
    game.phase=score==8?EC_EXITING:EC_PLAYING;game.anomaly=(ec_anomaly_t)anomaly;
    game.x=x;game.z=z;game.yaw=game.camera_yaw=yaw;game.score=game.hud_score=score;game.entry_exit=entry!=0;
    return 1;
}
