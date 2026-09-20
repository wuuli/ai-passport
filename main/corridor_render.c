#include "corridor_render.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define PI 3.14159265358979323846f
#include "corridor_palette.inc"
#include "corridor_sprite_lut.inc"
#include "corridor_notice.inc"
struct ec_renderer {
    uint32_t (*clock_us)(void);
    uint32_t stages[3];
    const uint8_t *sprite;
    size_t size;
    uint8_t *frame, *alpha; /* Separate bounded allocation fits fragmented internal RAM. */
    float depth[EC_WIDTH];
    int16_t wall_top[EC_WIDTH],wall_bottom[EC_WIDTH];
    uint8_t sprite_row[EC_HEIGHT];
    float sign_depth[3][EC_WIDTH];
    int16_t sign_top[3][EC_WIDTH], sign_bottom[3][EC_WIDTH];
    int frame_id, cell;
    ec_anomaly_t history[3];
    bool initialized;
};
static unsigned u16(const uint8_t *b) { return b[0]|((unsigned)b[1]<<8); }
static uint32_t u32(const uint8_t *b) { return u16(b)|((uint32_t)u16(b+2)<<16); }
static int mini(int a,int b) {return a<b?a:b;}
static int maxi(int a,int b) {return a>b?a:b;}
static int mod(int a,int b) {int r=a%b;return r<0?r+b:r;}
static bool decode(ec_renderer_t *r,unsigned id,bool validate_only) {
    if(id>=136)return false;
    size_t start=u32(r->sprite+24+id*4), end=id<135?u32(r->sprite+28+id*4):r->size;
    if(start<568||start>=end||end>r->size||end-start<4)return false;
    const uint8_t *b=r->sprite; size_t p=start;
    unsigned x=b[p++],y=b[p++],w=b[p++],h=b[p++];
    if(x+w>48||y+h>96)return false;
    if(!validate_only){memset(r->alpha,0,48*96/8);memset(r->frame,0,48*96);}
    if(!w||!h)return p==end;
    if(p+2>end)return false;
    unsigned count=u16(b+p);p+=2;
    if(!count||count>256||p+count*3>end)return false;
    uint8_t colors[256],alpha[256];
    if(!validate_only)for(unsigned i=0;i<count;++i){
        unsigned c=u16(b+p+i*3);
        colors[i]=ec_sprite_lut[c];
        alpha[i]=b[p+i*3+2];
    }
    p+=count*3;
    for(unsigned row=0;row<h;++row){
        if(p>=end)return false;
        unsigned spans=b[p++],xx=x;
        for(unsigned s=0;s<spans;++s){
            if(p+2>end)return false;
            xx+=b[p++];unsigned n=b[p++];
            if(xx+n>x+w||p+n>end)return false;
            for(unsigned k=0;k<n;++k){
                unsigned c=b[p++];if(c>=count)return false;
                if(!validate_only){unsigned at=(y+row)*48+xx+k;r->frame[at]=colors[c];if(alpha[c]>=96)r->alpha[at/8]|=(uint8_t)(1u<<(at%8));}
            }
            xx+=n;
        }
    }
    return p==end;
}
ec_renderer_t *ec_renderer_create(const uint8_t *sprite,size_t size) {
    if(!sprite||size<568||memcmp(sprite,"ECSP",4)||sprite[4]!=1||sprite[5]!=8||sprite[6]!=16||sprite[7]!=17||u16(sprite+8)!=48||u16(sprite+10)!=96||u32(sprite+24)!=568)return NULL;
    ec_renderer_t *r=calloc(1,sizeof(*r));if(!r)return NULL;
    r->sprite=sprite;r->size=size;r->frame_id=-1;
    for(unsigned i=0;i<136;++i)if(!decode(r,i,true)){free(r);return NULL;}
    r->frame=calloc(1,48*96+48*96/8);
    if(!r->frame){free(r);return NULL;}
    r->alpha=r->frame+48*96;
    return r;
}
void ec_renderer_destroy(ec_renderer_t *r){if(r)free(r->frame);free(r);}
void ec_renderer_set_clock(ec_renderer_t *r,uint32_t (*clock_us)(void)){r->clock_us=clock_us;}
void ec_renderer_profile(const ec_renderer_t *r,uint32_t stages_us[3]){memcpy(stages_us,r->stages,sizeof(r->stages));}
size_t ec_renderer_work_bytes(void){return sizeof(ec_renderer_t)+48*96+48*96/8;}

/* Compact glyphs for the in-world guide and direction signs, columns top-first. */
static const uint8_t glyphs[36][5]={
 {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},
 {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30},
 {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},
 {127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},
 {127,8,20,34,65},{127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},
 {127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},{7,8,112,8,7},{97,81,73,69,67}
};
static bool text_at(const char *s,int x,int y){
    if(x<0||y<0||y>=7)return false;
    unsigned n=(unsigned)x/6;if(n>=strlen(s)||x%6==5)return false;
    unsigned char c=(unsigned char)s[n];int id=c>='0'&&c<='9'?c-'0':c>='A'&&c<='Z'?c-'A'+10:-1;
    return id>=0 && (glyphs[id][x%6]&(1u<<y));
}
static uint8_t notice(int u,int v){
    if(u<3||u>124||v<3||v>157)return 28;
    if(ec_notice_bits[v*16+u/8]&(0x80u>>(u%8)))return 12;
    if(v<25)return 153;
    return 118;
}
/* Classify fixtures once per ray. Plain painted walls avoid subpixel grout
 * shimmer on the native screen; depth lighting and skirting retain shape. */
typedef struct {
    int poster,poster_u,notice_u,door_u;
    bool vent,inverted,eyes;
    int kind;
} wall_sample_t;
static wall_sample_t wall_sample(int side,int lz,ec_anomaly_t a){
    wall_sample_t w={.poster=-1,.notice_u=-1,.door_u=-1,
        .inverted=a==EC_POSTER_INVERTED,.eyes=a==EC_POSTER_EYES};
    if(side==0){
        if(lz>=-4100&&lz<=-2900)w.notice_u=(-2900-lz)*128/1200;
        w.vent=(lz>=-6225&&lz<=-5375)||(lz>=-18425&&lz<=-17575)||(a==EC_EXTRA_VENT&&lz>=-13425&&lz<=-12575);
        if(a!=EC_DOOR_MISSING&&lz>=-8500&&lz<=-7500)w.door_u=-7500-lz;
    }
    if(side==1){
        const int centers[]={-5000,-8000,-11000,-16000,-20000};
        for(int i=0;i<5;++i)if(abs(lz-centers[i])<=425){w.poster=i;w.poster_u=(lz-centers[i]+425)*64/850;break;}
    }
    w.kind=w.notice_u>=0?1:w.vent?2:w.door_u>=0?3:w.poster>=0?4:0;
    return w;
}
static uint8_t wall_color(const wall_sample_t *w,int height){
    if(height<120)return (uint8_t)(45+maxi(0,height)/6);
    if(!w->kind)return 117;
    if(w->kind==1&&height>=975&&height<=2325)return notice(w->notice_u,(2325-height)*160/1350);
    if(w->kind==2&&height>=2075&&height<=2525)return mod(height,50)<18?23:85;
    if(w->kind==3&&height<2150){
        int u=w->door_u;
        if(u<35||u>965||height>2100)return 23;
        if(u>800&&u<930&&height>920&&height<970)return 115;
        if(u>200&&u<800&&height<450&&height>200)return mod(height,45)<18?30:70;
        return (uint8_t)(77+u/65);
    }
    if(w->kind==4&&height>=950&&height<=2150){
        int i=w->poster,u=w->poster_u,v=(2150-height)*96/1200;
        if(u<2||u>61||v<2||v>93)return 25;
        if(i==0&&w->inverted){u=63-u;v=95-v;}
        if(v<18)return text_at(i==1?"SECURITY":"SAFETY",u-7,v-6)?124:242;
        if(i==1&&w->eyes&&v>=33&&v<=51){
            int d=mini(abs(u-20),abs(u-44));
            if(d*d+(v-42)*(v-42)<62)return d<3?8:124;
        }
        if(v>73)return (v%6<2&&u>7&&u<55)?48:118;
        if(i==1){if(u>10&&u<49&&v>31&&v<52)return 29;if(u>26&&u<33&&v>=52&&v<60)return 40;}
        else {int dx=u-32,dy=v-39;if(dx*dx+dy*dy<75)return 242;if(abs(dx)<7&&v>48&&v<65)return 242;}
        return 118;
    }
    return 117;
}
static const float walls[6][4]={
 {-1.6f,-28.4f,-1.6f,1.2f},{1.6f,-25.2f,1.6f,4.4f},{-1.6f,-28.4f,4.8f,-28.4f},
 {1.6f,-25.2f,4.8f,-25.2f},{-4.8f,1.2f,-1.6f,1.2f},{-4.8f,4.4f,1.6f,4.4f}
};
/* Fixed-point row setup avoids thousands of software floating-point
 * operations per frame on the ESP32-C3, which has no hardware FPU. */
static void draw_surfaces(ec_renderer_t *r,uint8_t *out,
    float cam_x,float cam_z,float fx,float fz,float rx,float rz,
    int32_t origin_qx,int32_t origin_qz,const int32_t cell_qx[3],const int32_t cell_qz[3]){
    int32_t cx=(int32_t)lroundf(cam_x*65536),cz=(int32_t)lroundf(cam_z*65536);
    int32_t left_x=(int32_t)lroundf((fx-rx*.6f)*16777216);
    int32_t left_z=(int32_t)lroundf((fz-rz*.6f)*16777216);
    int32_t right_x=(int32_t)lroundf(rx*16777216),right_z=(int32_t)lroundf(rz*16777216);
    for(int y=0;y<EC_HEIGHT;++y){
        bool floor=y>=160;int height=floor?310:280,dy=maxi(1,abs(y-160));
        int32_t distance=(height*65536)/dy;
        int32_t xx=cx+(int32_t)(((int64_t)left_x*distance)/16777216);
        int32_t zz=cz+(int32_t)(((int64_t)left_z*distance)/16777216);
        int32_t sx=(int32_t)(((int64_t)right_x*distance)/(200LL*16777216));
        int32_t sz=(int32_t)(((int64_t)right_z*distance)/(200LL*16777216));
        int shade=mini(15,maxi(1,16000*dy/(1000*dy+height*52)-1));
        for(int x=0;x<EC_WIDTH;++x,xx+=sx,zz+=sz){
            if(y>=r->wall_top[x]&&y<=r->wall_bottom[x])continue;
            int px=(xx-origin_qx)>>6,pz=(zz-origin_qz)>>6;
            int ax=xx>>6,az=zz>>6;
            int lx=px,lz=pz,ci=1;
            if(lx>4915)ci=2;else if(lx< -4915)ci=0;
            lx=(xx-cell_qx[ci])>>6;lz=(zz-cell_qz[ci])>>6;
            bool red=r->history[ci]==EC_RED_LIGHTS&&lz>=-22528&&lz<=-2048&&abs(lx)<1690;
            uint8_t c;
            if(floor){
                bool tactile=(abs(lx)<175&&lz>=-27443&&lz<=2867)||(abs(lz+27443)<175&&lx>0)||(abs(lz-2867)<175&&lx<0);
                if(tactile)c=(mod(abs(lx)<175?lx:lz,60)<16)?146:154;
                else {int noise=(int)(((uint32_t)(ax>>4)*73856093u)^((uint32_t)(az>>4)*19349663u));c=(uint8_t)(86+(noise&7));if(mod(ax,614)<5||mod(az,614)<5)c=71;}
            }else{
                bool lamp=(abs(lx)<205&&abs(mod(lz-1843+1843,3686)-1843)<716)||
                    (abs(lz+27443)<205&&abs(lx-3686)<716)||(abs(lz-2867)<205&&abs(lx+3686)<716);
                c=lamp?127:mod(az,1228)<6?94:111;
            }
            out[y*EC_WIDTH+x]=(red?ec_red:ec_shade)[shade][c];
        }
    }
}
void ec_renderer_draw(ec_renderer_t *r,const ec_game_t *g,uint8_t *image){
    if(!r||!g||!image)return;
    uint32_t started=r->clock_us?r->clock_us():0,mark=started;
    memset(r->stages,0,sizeof(r->stages));
    memcpy(image,ec_palette,EC_PALETTE_BYTES);uint8_t *out=image+EC_PALETTE_BYTES;
    if(!r->initialized||abs(g->cell-r->cell)>1||g->phase==EC_TITLE){memset(r->history,0,sizeof(r->history));r->initialized=true;}
    else if(g->cell==r->cell+1){r->history[0]=r->history[1];r->history[1]=r->history[2];r->history[2]=EC_NORMAL;}
    else if(g->cell==r->cell-1){r->history[2]=r->history[1];r->history[1]=r->history[0];r->history[0]=EC_NORMAL;}
    r->cell=g->cell;r->history[1]=g->anomaly;
    float fx=sinf(g->camera_yaw),fz=-cosf(g->camera_yaw),rx=-fz,rz=fx;
    float origin_x=g->cell*9.6f,origin_z=-g->cell*29.6f;
    float cam_x=roundf((g->x+origin_x)*1024)/1024,cam_z=roundf((g->z+origin_z)*1024)/1024;
    int32_t origin_qx=(int32_t)lroundf(origin_x*65536),origin_qz=(int32_t)lroundf(origin_z*65536);
    int32_t cell_qx[3],cell_qz[3];
    for(int i=0;i<3;++i){cell_qx[i]=(int32_t)lroundf((g->cell+i-1)*9.6f*65536);cell_qz[i]=(int32_t)lroundf(-(g->cell+i-1)*29.6f*65536);}
    /* Q10 positions and Q14 ray directions keep the inner ray loop in the
     * C3 integer divider. World coordinates preserve connector continuity. */
    int32_t cam_qx=(int32_t)lroundf(cam_x*1024),cam_qz=(int32_t)lroundf(cam_z*1024);
    int32_t fq_x=(int32_t)lroundf(fx*16384),fq_z=(int32_t)lroundf(fz*16384);
    int32_t rq_x=(int32_t)lroundf(rx*16384),rq_z=(int32_t)lroundf(rz*16384);
    int32_t world_walls[18][4];
    for(int ci=0;ci<3;++ci)for(int w=0;w<6;++w)for(int k=0;k<4;++k){
        float origin=k%2?-(g->cell+ci-1)*29.6f:(g->cell+ci-1)*9.6f;
        world_walls[ci*6+w][k]=(int32_t)lroundf((walls[w][k]+origin)*1024);
    }
    for(int x=0;x<EC_WIDTH;++x){
        int32_t dx=fq_x+rq_x*(x-120)/200,dz=fq_z+rq_z*(x-120)/200;
        int32_t best_q=65536,hit_z=cam_qz;int side=0,cell=1;
        for(int i=0;i<18;++i){
            const int32_t *w=world_walls[i];int32_t t,along;
            if(i%6<2){
                if(!dx)continue;
                t=(w[0]-cam_qx)*16384/dx;
                if(t<=20||t>=best_q)continue;
                along=cam_qz+(int32_t)(((int64_t)dz*t)>>14);
                if(along<w[1]||along>w[3])continue;
                hit_z=along;
            }else{
                if(!dz)continue;
                t=(w[1]-cam_qz)*16384/dz;
                if(t<=20||t>=best_q)continue;
                along=cam_qx+(int32_t)(((int64_t)dx*t)>>14);
                if(along<w[0]||along>w[2])continue;
                hit_z=w[1];
            }
            best_q=t;side=i%6;cell=i/6;
        }
        float best=best_q/1024.0f;r->depth[x]=best;
        int top=maxi(0,160-(286720+best_q-1)/best_q),bottom=mini(319,160+317440/best_q);
        r->wall_top[x]=(int16_t)top;r->wall_bottom[x]=(int16_t)bottom;
        int shade=mini(15,maxi(1,(int)(16/(1+best*.052f))-1));
        /* A fixed face contrast keeps perpendicular plain walls distinguishable. */
        if(side>=2)shade=maxi(0,shade-1);
        int local_z=hit_z-(cell_qz[cell]>>6);
        bool red=r->history[cell]==EC_RED_LIGHTS&&local_z>=-22528&&local_z<=-2048;
        int step=best_q*320,wy=1550*65536+(160-top)*step;
        int lz=local_z*1000/1024;
        wall_sample_t sample=wall_sample(side,lz,r->history[cell]);
        const uint8_t *lighting=(red?ec_red:ec_shade)[shade];
        for(int y=top;y<=bottom;++y,wy-=step)out[y*EC_WIDTH+x]=lighting[wall_color(&sample,wy>>16)];
    }
    if(r->clock_us){uint32_t now=r->clock_us();r->stages[1]=now-mark;mark=now;}
    draw_surfaces(r,out,cam_x,cam_z,fx,fz,rx,rz,origin_qx,origin_qz,cell_qx,cell_qz);
    if(r->clock_us){uint32_t now=r->clock_us();r->stages[0]=now-mark;mark=now;}
    /* Draw signs after floor/ceiling so floor pixels need no per-sign depth
     * tests. Vertical text sampling is a fixed-point increment, not a float
     * divide at every sign pixel. */
    for(int x=0;x<EC_WIDTH;++x){
        int32_t dx=fq_x+rq_x*(x-120)/200,dz=fq_z+rq_z*(x-120)/200;
        const float signs[]={-2,-14,-22.5f};
        for(int k=0;k<3;++k){
            r->sign_depth[k][x]=10000;r->sign_top[k][x]=0;r->sign_bottom[k][x]=-1;
            if(!dz)continue;
            int32_t sign_z=(int32_t)lroundf((signs[k]+origin_z)*1024);
            int32_t tq=(sign_z-cam_qz)*16384/dz;
            if(tq<82||tq>=r->depth[x]*1024)continue;
            int32_t xx=cam_qx+(int32_t)(((int64_t)dx*tq)>>14)-(origin_qx>>6);
            if(abs(xx)>691)continue;
            int a=maxi(0,160-(225280+tq-1)/tq),b=mini(319,160-(147456+tq-1)/tq);
            if(a>b)continue;
            r->sign_depth[k][x]=tq/1024.0f;r->sign_top[k][x]=(int16_t)a;r->sign_bottom[k][x]=(int16_t)b;
            int u=(xx+691)*96/1382,step=tq*8960/380;
            int vq=(1100*28*65536)/380-(160-a)*step;
            for(int y=a;y<=b;++y,vq+=step){
                int v=vq/65536;
                bool ink=text_at("EXIT",(u-8)/2,(v-7)/2)||text_at((const char[2]){(char)('0'+g->score),0},(u-69)/2,(v-7)/2);
                out[y*EC_WIDTH+x]=ink?12:154;
            }
        }
    }
    if(r->clock_us){uint32_t now=r->clock_us();r->stages[1]+=now-mark;mark=now;}
    if(g->anomaly==EC_ABSENT_NPC)return;
    float nx=g->npc_x+origin_x-cam_x,nz=g->npc_z+origin_z-cam_z,d=nx*fx+nz*fz,side=nx*rx+nz*rz;
    if(d<.25f)return;
    float rel=atan2f(-nx,-nz)-g->npc_heading;
    unsigned dir=(unsigned)mod((int)floorf(rel*4/PI+.5f),8);
    unsigned row=(g->anomaly==EC_STARING_NPC||g->npc_turn>0)?16:(unsigned)(g->npc_distance/(.85f*1.067f)*16)%16;
    unsigned frame=row*8+dir;
    if((int)frame!=r->frame_id){if(!decode(r,frame,false))return;r->frame_id=(int)frame;}
    float h=2*(g->anomaly==EC_TALL_NPC?1.45f:1),sh=h*200/d,sw=200/d,foot=160+310/d,left=120+side*200/d-sw/2,top=foot-sh*.95f;
    int shade=mini(15,maxi(1,(int)(16/(1+d*.052f))-1));
    bool red=g->anomaly==EC_RED_LIGHTS&&g->npc_z>=-22&&g->npc_z<=-2;
    int sprite_top=maxi(0,(int)top),sprite_bottom=mini(319,(int)(top+sh));
    for(int y=sprite_top;y<=sprite_bottom;++y)r->sprite_row[y]=(uint8_t)mini(95,maxi(0,(int)((y-top)*96/sh)));
    for(int x=maxi(0,(int)left);x<=mini(239,(int)(left+sw));++x){
        if(d>=r->depth[x])continue;
        int u=mini(47,maxi(0,(int)((x-left)*48/sw)));
        for(int y=sprite_top;y<=sprite_bottom;++y){
            int at=r->sprite_row[y]*48+u;
            if(!(r->alpha[at/8]&(1u<<(at%8))))continue;
            bool occluded=false;
            for(int k=0;k<3;++k)if(d>=r->sign_depth[k][x]&&y>=r->sign_top[k][x]&&y<=r->sign_bottom[k][x])occluded=true;
            if(occluded)continue;
            out[y*240+x]=(red?ec_red:ec_shade)[shade][r->frame[at]];
        }
    }
    if(r->clock_us)r->stages[2]=r->clock_us()-mark;
}
