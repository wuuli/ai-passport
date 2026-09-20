#include "corridor_game.h"
#include "corridor_render.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned hash(const uint8_t *p,size_t n){unsigned h=2166136261u;for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h;}
int main(void){
    FILE *f=fopen("assets/images/exit-corridor/commuter-device.bin","rb");assert(f);fseek(f,0,SEEK_END);size_t n=(size_t)ftell(f);rewind(f);
    uint8_t *asset=malloc(n);assert(asset&&fread(asset,1,n,f)==n);fclose(f);
    assert(!ec_renderer_create(asset,24));uint8_t magic=asset[0];asset[0]=0;assert(!ec_renderer_create(asset,n));asset[0]=magic;
    assert(!ec_renderer_create(asset,n-1));
    ec_renderer_t *r=ec_renderer_create(asset,n);assert(r);
    uint8_t *b=malloc(EC_IMAGE_BYTES+32),*old=malloc(EC_IMAGE_BYTES);assert(b&&old);memset(b,0xa5,EC_IMAGE_BYTES+32);
    ec_game_t g;ec_game_init(&g,1);g.phase=EC_PLAYING;g.npc_z=-5;
    ec_renderer_draw(r,&g,b);memcpy(old,b,EC_IMAGE_BYTES);ec_renderer_draw(r,&g,b);assert(!memcmp(old,b,EC_IMAGE_BYTES));
    for(unsigned a=1;a<=8;++a){
        bool different=false;
        for(int p=0;p<12;++p){g.z=-3.8f-(p/2)*2;g.camera_yaw=p%2?-1.2f:1.2f;g.anomaly=EC_NORMAL;ec_renderer_draw(r,&g,b);unsigned normal=hash(b,EC_IMAGE_BYTES);
            g.anomaly=(ec_anomaly_t)a;ec_renderer_draw(r,&g,b);different|=normal!=hash(b,EC_IMAGE_BYTES);
            if(a==EC_STARING_NPC){g.npc_heading=2;g.npc_distance=.2f;ec_renderer_draw(r,&g,b);different|=normal!=hash(b,EC_IMAGE_BYTES);}
        }if(!different)fprintf(stderr,"anomaly %u unchanged\n",a);assert(different);
    }
    for(int i=0;i<32;++i)assert(b[EC_IMAGE_BYTES+i]==0xa5);
    unsigned seam_cases=0;
    for(int portal=0;portal<2;++portal)for(int yaw=0;yaw<8;++yaw)for(int pair=0;pair<4;++pair){
        ec_game_init(&g,1);g.x=portal?-4.81f:4.81f;g.z=portal?2.8f:-26.8f;
        g.camera_yaw=yaw*3.14159265358979323846f/4;g.anomaly=pair&1?EC_RED_LIGHTS:EC_NORMAL;
        ec_renderer_draw(r,&g,b);memcpy(old,b,EC_IMAGE_BYTES);
        g.phase=EC_PLAYING;g.cell=portal?-1:1;g.x-=g.cell*9.6f;g.z+=g.cell*29.6f;
        g.anomaly=pair&2?EC_RED_LIGHTS:EC_NORMAL;g.score=1;
        ec_renderer_draw(r,&g,b);
        if(memcmp(old,b,EC_IMAGE_BYTES))fprintf(stderr,"seam mismatch portal%d yaw%d pair%d\n",portal,yaw,pair);
        assert(!memcmp(old,b,EC_IMAGE_BYTES));++seam_cases;
    }
    printf("Corridor seams: %u same-camera portal cases PASS\n",seam_cases);
    /* Face a fixture-free span; tangential motion must not introduce grout flicker. */
    ec_game_init(&g,1);g.phase=EC_PLAYING;g.anomaly=EC_ABSENT_NPC;
    g.x=0;g.z=-10.5f;g.yaw=g.camera_yaw=-3.14159265358979323846f/2;
    ec_renderer_draw(r,&g,b);memcpy(old,b,EC_IMAGE_BYTES);
    for(int step=-4;step<=4;++step){
        g.z=-10.5f+step*.025f;ec_renderer_draw(r,&g,b);
        for(int y=50;y<=250;++y)for(int x=100;x<=140;++x){
            int at=EC_PALETTE_BYTES+y*EC_WIDTH+x;
            assert(b[at]==old[at]);
            if(x==120)assert(b[at]==102); /* Palette 117 with the existing 1.6 m lighting. */
        }
    }
    printf("Corridor plain walls: 8241 fixture-free pixels stable at 9 positions PASS\n");
    /* Equal-distance rays on either side of each outer corner retain face contrast. */
    for(int end=0;end<2;++end){
        g.x=0;g.z=end?2.8f:-26.8f;
        g.yaw=g.camera_yaw=(end?3:-1)*3.14159265358979323846f/4;
        ec_renderer_draw(r,&g,b);
        for(int y=110;y<=210;++y){
            int row=EC_PALETTE_BYTES+y*EC_WIDTH;
            assert(b[row+100]>=b[row+140]+5);
        }
    }
    printf("Corridor corner faces: both outer corners retain visible contrast PASS\n");
    ec_game_init(&g,1);g.npc_z=-7;ec_renderer_draw(r,&g,b);
    f=fopen("/tmp/corridor-native.ppm","wb");assert(f);fprintf(f,"P6\n240 320\n255\n");
    for(int i=0;i<240*320;++i){const uint8_t *c=b+b[1024+i]*4;uint8_t rgb[]={c[2],c[1],c[0]};fwrite(rgb,1,3,f);}fclose(f);
    printf("Corridor renderer: real asset, malformed/truncated rejection, deterministic frames, 8 anomaly views, bounds PASS; work=%zu bytes\n",ec_renderer_work_bytes());
    ec_renderer_destroy(r);free(asset);free(b);free(old);
}
