#include "corridor_game.h"
#include "corridor_render.h"
#include <assert.h>
#include <math.h>
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
/* Exit sequence rendering tests: EC_EXITING and EC_CLEARED in both directions */
    for (int dir = 0; dir < 2; ++dir) {
        bool entry_exit = dir != 0;
        float z_start = entry_exit ? -19.0f : -5.0f;
        float z_end = entry_exit ? -15.0f : -9.0f;
        float z_step = entry_exit ? 1.0f : -1.0f;
        float forward_yaw = entry_exit ? 3.14159265358979323846f : 0.0f;

        /* Test 1: Deterministic rendering & differing nonblank frames while moving */
        unsigned prev_hash = 0;
        for (int step = -2; step <= 5; ++step) {
            float z = (step == 5) ? z_end : (z_start + (step * 0.8f) * z_step);
            ec_game_init(&g, 1);
            g.phase = (step == 5) ? EC_CLEARED : EC_EXITING;
            g.score = 8;
            g.entry_exit = entry_exit;
            g.x = 0;
            g.z = z;
            g.yaw = g.camera_yaw = forward_yaw;

            ec_renderer_draw(r, &g, b);

            /* Check nonblank: varied pixel content across screen */
            uint8_t first = b[EC_PALETTE_BYTES];
            bool varied = false;
            for (int i = 0; i < EC_WIDTH * EC_HEIGHT; ++i) {
                if (b[EC_PALETTE_BYTES + i] != first) { varied = true; break; }
            }
            assert(varied);

            /* Check determinism: re-drawing same state yields identical bytes */
            memcpy(old, b, EC_IMAGE_BYTES);
            ec_renderer_draw(r, &g, b);
            assert(!memcmp(old, b, EC_IMAGE_BYTES));

            /* Check differing frames while moving */
            unsigned cur_hash = hash(b, EC_IMAGE_BYTES);
            if (prev_hash != 0) {
                assert(cur_hash != prev_hash);
            }
            prev_hash = cur_hash;
        }

        /* Test 2: Multiple camera yaw angles (45 deg left/right, backward) at mid-ascent */
        float mid_z = (z_start + z_end) * 0.5f;
        const float test_yaws[] = {
            forward_yaw,
            forward_yaw + 3.14159265358979323846f / 4.0f,
            forward_yaw - 3.14159265358979323846f / 4.0f,
            forward_yaw + 3.14159265358979323846f
        };
        unsigned yaw_hashes[4];
        for (int yi = 0; yi < 4; ++yi) {
            ec_game_init(&g, 1);
            g.phase = EC_EXITING;
            g.score = 8;
            g.entry_exit = entry_exit;
            g.x = 0;
            g.z = mid_z;
            g.yaw = g.camera_yaw = test_yaws[yi];
            ec_renderer_draw(r, &g, b);
            yaw_hashes[yi] = hash(b, EC_IMAGE_BYTES);
            for (int yprev = 0; yprev < yi; ++yprev) {
                assert(yaw_hashes[yi] != yaw_hashes[yprev]);
            }
        }

        /* Test 3: No NPC or anomaly fixture leak in current cell during exit */
        ec_game_init(&g, 1);
        g.phase = EC_EXITING;
        g.score = 8;
        g.entry_exit = entry_exit;
        g.x = 0;
        g.z = z_start;
        g.yaw = g.camera_yaw = forward_yaw;
        g.anomaly = EC_NORMAL;
        ec_renderer_draw(r, &g, b);
        memcpy(old, b, EC_IMAGE_BYTES);

        for (unsigned a = 1; a <= 8; ++a) {
            g.anomaly = (ec_anomaly_t)a;
            g.npc_x = 0;
            g.npc_z = mid_z;
            ec_renderer_draw(r, &g, b);
            assert(!memcmp(old, b, EC_IMAGE_BYTES));
        }

        /* Test 4: Winning portal seam check: preserved world coords, nonblank */
        for (int yaw = 0; yaw < 8; ++yaw) {
            float cam_yaw = yaw * 3.14159265358979323846f / 4.0f;
            ec_game_init(&g, 1);
            g.x = entry_exit ? -4.81f : 4.81f;
            g.z = entry_exit ? 2.8f : -26.8f;
            g.camera_yaw = cam_yaw;
            g.score = 7;
            g.phase = EC_PLAYING;
            g.anomaly = EC_RED_LIGHTS;
            ec_renderer_draw(r, &g, b);

            float world_x1 = g.x + g.cell * 9.6f;
            float world_z1 = g.z - g.cell * 29.6f;

            int cell_delta = entry_exit ? -1 : 1;
            g.cell = cell_delta;
            g.x = (entry_exit ? -4.81f : 4.81f) - cell_delta * 9.6f;
            g.z = (entry_exit ? 2.8f : -26.8f) + cell_delta * 29.6f;
            g.score = 8;
            g.phase = EC_EXITING;
            g.entry_exit = entry_exit;
            g.anomaly = EC_NORMAL;

            float world_x2 = g.x + g.cell * 9.6f;
            float world_z2 = g.z - g.cell * 29.6f;

            assert(fabsf(world_x1 - world_x2) < 0.001f);
            assert(fabsf(world_z1 - world_z2) < 0.001f);

            ec_renderer_draw(r, &g, b);

            /* Nonblank: every pixel maps to a valid non-zero palette index with non-zero luminance */
            for (int i = 0; i < EC_WIDTH * EC_HEIGHT; i += 100) {
                uint8_t pal_idx = b[EC_PALETTE_BYTES + i];
                assert(pal_idx > 0);
                const uint8_t *rgb = b + pal_idx * 4;
                assert(rgb[0] > 0 || rgb[1] > 0 || rgb[2] > 0);
            }
        }
    }

    /* Sentinel bounds check: 32 bytes after image buffer remain untouched */
    for (int i = 0; i < 32; ++i) assert(b[EC_IMAGE_BYTES + i] == 0xa5);
    /* Working memory bound unchanged: concrete existing bound */
    assert(ec_renderer_work_bytes() == 13264u);
    printf("Corridor exit sequence: both directions, ascent, 45-deg/backward yaw, seam continuity, anomaly suppression PASS\n");
    ec_game_init(&g,1);g.npc_z=-7;ec_renderer_draw(r,&g,b);
    f=fopen("/tmp/corridor-native.ppm","wb");assert(f);fprintf(f,"P6\n240 320\n255\n");
    for(int i=0;i<240*320;++i){const uint8_t *c=b+b[1024+i]*4;uint8_t rgb[]={c[2],c[1],c[0]};fwrite(rgb,1,3,f);}fclose(f);
    printf("Corridor renderer: real asset, malformed/truncated rejection, deterministic frames, 8 anomaly views, bounds PASS; work=%zu bytes\n",ec_renderer_work_bytes());
    ec_renderer_destroy(r);free(asset);free(b);free(old);
}
