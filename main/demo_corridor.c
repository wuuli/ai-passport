#include "demo.h"
#include "corridor_game.h"
#include "corridor_render.h"
#include "duel_io.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/draw/lv_image_decoder_private.h"
#include <stdlib.h>
#include <string.h>

extern const uint8_t ec_sprite_start[] asm("_binary_commuter_device_bin_start");
extern const uint8_t ec_sprite_end[] asm("_binary_commuter_device_bin_end");
static lv_obj_t *screen,*image_obj,*status,*battery,*overlay,*subtitle,*title,*caption,*prompt,*footer,*rule,*bar;
static uint8_t *pixels;
static ec_renderer_t *renderer;
static ec_game_t game;
static lv_image_dsc_t image;
static int64_t last_tick,last_render,last_log;
static unsigned frame_count;
static uint64_t render_total,tick_total,refresh_total;
static int64_t refresh_started;
static unsigned refresh_count,decoded_chunks;
static void refresh_event(lv_event_t *event){
    if(lv_event_get_code(event)==LV_EVENT_RENDER_START)refresh_started=esp_timer_get_time();
    else if(refresh_started){refresh_total+=(uint64_t)(esp_timer_get_time()-refresh_started);++refresh_count;refresh_started=0;}
}
static bool visible,dirty;
static const char *TAG="corridor";
/* The generic I8 decoder expands each scanline to ARGB8888. Convert directly
 * to the panel's opaque RGB565 in five-row chunks, still through LVGL's SPI
 * owner. This buffer is synchronous and shared only inside the LVGL task. */
static lv_image_decoder_t *image_decoder;
static lv_draw_buf_t decoded_rows;
static uint16_t scanout[EC_WIDTH*5] __attribute__((aligned(64)));
static uint16_t palette565[256];
static lv_result_t image_info(lv_image_decoder_t *dec,lv_image_decoder_dsc_t *dsc,lv_image_header_t *header){
    (void)dec;if(dsc->src!=&image||!pixels)return LV_RESULT_INVALID;
    *header=image.header;header->cf=LV_COLOR_FORMAT_RGB565;header->stride=EC_WIDTH*2;return LV_RESULT_OK;
}
static lv_result_t image_open(lv_image_decoder_t *dec,lv_image_decoder_dsc_t *dsc){
    (void)dec;if(dsc->src!=&image||!pixels)return LV_RESULT_INVALID;
    dsc->decoded=NULL;dsc->args.no_cache=true;return LV_RESULT_OK;
}
static lv_result_t image_area(lv_image_decoder_t *dec,lv_image_decoder_dsc_t *dsc,const lv_area_t *full,lv_area_t *area){
    (void)dec;
    if(!pixels||dsc->src!=&image)return LV_RESULT_INVALID;
    int y=area->y1==LV_COORD_MIN?full->y1:area->y2+1;
    if(y>full->y2||full->x1<0||full->x2>=EC_WIDTH||y<0||full->y2>=EC_HEIGHT)return LV_RESULT_INVALID;
    *area=*full;area->y1=y;area->y2=y+4<full->y2?y+4:full->y2;
    unsigned width=(unsigned)(area->x2-area->x1+1),height=(unsigned)(area->y2-y+1);
    for(unsigned row=0;row<height;++row){
        const uint8_t *src=pixels+EC_PALETTE_BYTES+(y+row)*EC_WIDTH+area->x1;
        for(unsigned x=0;x<width;++x)scanout[row*EC_WIDTH+x]=palette565[src[x]];
    }
    if(lv_draw_buf_init(&decoded_rows,width,height,LV_COLOR_FORMAT_RGB565,EC_WIDTH*2,scanout,sizeof(scanout))!=LV_RESULT_OK)return LV_RESULT_INVALID;
    ++decoded_chunks;dsc->decoded=&decoded_rows;return LV_RESULT_OK;
}

extern const lv_font_t corridor_font;
extern const lv_font_t corridor_title_font;

static lv_obj_t *label(lv_obj_t *parent,const char *text,int y,const lv_font_t *font,lv_color_t color){
    lv_obj_t *obj=lv_label_create(parent);
    lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,color,0);
    lv_label_set_text(obj,text);lv_obj_align(obj,LV_ALIGN_TOP_MID,0,y);return obj;
}
static lv_opa_t prompt_opacity=LV_OPA_COVER;
static int presentation_phase=-1;
static int64_t presentation_started;
static int displayed_battery=-2;
static void update_battery(void){
    if(!battery)return;
    int charge=duel_io_battery();
    if(charge==displayed_battery)return;
    displayed_battery=charge;
    if(charge>=0)lv_label_set_text_fmt(battery,"%d%%",charge);
    else lv_label_set_text(battery,"--");
}
static void update_title_prompt(int64_t now){
    if(!visible||!renderer||!prompt||(game.phase!=EC_TITLE&&game.phase!=EC_CLEARED))return;
    float t=(now-presentation_started)/800000.0f;
    if(t<0)t=0;
    if(t>1)t=1;
    t=t*t*(3-2*t);
    lv_opa_t opacity=(lv_opa_t)(LV_OPA_60+(LV_OPA_COVER-LV_OPA_60)*t);
    if(opacity!=prompt_opacity){lv_obj_set_style_text_opa(prompt,opacity,0);prompt_opacity=opacity;}
}
static uint32_t render_clock(void){return (uint32_t)esp_timer_get_time();}
static void hud(void){
    if(!screen)return;
    update_battery();
    if(!renderer){lv_label_set_text(status,"长按 OK 返回");return;}
    if(presentation_phase!=(int)game.phase){
        presentation_phase=game.phase;presentation_started=esp_timer_get_time();
        bool world=game.phase==EC_PLAYING||game.phase==EC_EXITING;
        bool end=game.phase==EC_CLEARED;
        lv_color_t ink=lv_color_hex(end?0x242b2b:0xefeee8);
        lv_color_t secondary=lv_color_hex(end?0x586161:0xb6bdbd);
        lv_obj_set_style_bg_opa(bar,world?LV_OPA_80:LV_OPA_TRANSP,0);
        lv_obj_set_style_text_color(battery,ink,0);
        lv_obj_set_style_bg_color(overlay,lv_color_hex(end?0xf3f4f0:0x080d0d),0);
        lv_obj_set_style_bg_opa(overlay,end?LV_OPA_90:LV_OPA_70,0);
        lv_obj_set_style_text_color(title,ink,0);
        lv_obj_set_style_text_color(subtitle,secondary,0);
        lv_obj_set_style_text_color(caption,secondary,0);
        lv_obj_set_style_text_color(footer,secondary,0);
        lv_obj_set_style_text_color(prompt,end?ink:lv_color_hex(0xe7cd70),0);
        lv_obj_set_style_bg_color(rule,lv_color_hex(end?0x8c9694:0xd6b954),0);
        prompt_opacity=LV_OPA_60;lv_obj_set_style_text_opa(prompt,prompt_opacity,0);
    }
    if(game.phase==EC_PLAYING||game.phase==EC_EXITING){
        lv_obj_add_flag(overlay,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(status,"出口 %u  %s",game.hud_score,game.turning&&game.walking?"转身中":game.walking?"行走中":"OK 行走");
    }else{
        lv_obj_remove_flag(overlay,LV_OBJ_FLAG_HIDDEN);
        if(game.phase==EC_TITLE){
            lv_label_set_text(subtitle,"地下通道");
            lv_label_set_text(title,"8号出口");
            lv_obj_align(caption,LV_ALIGN_TOP_MID,0,160);
            lv_label_set_text(caption,"顶部键：左转\n中部键：右转\n底部 OK 键：行走／停步\n拐角自动转向，停后按 OK");
            lv_label_set_text(prompt,"按 OK 进入");
            lv_obj_remove_flag(prompt,LV_OBJ_FLAG_HIDDEN);
        }else{
            lv_label_set_text(subtitle,"");
            lv_label_set_text(title,"8号出口");
            lv_obj_align(caption,LV_ALIGN_TOP_MID,0,166);
            lv_label_set_text(caption,"你已走出通道");
            lv_label_set_text(prompt,"按 OK 再走一次");
            lv_obj_remove_flag(prompt,LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(status,"");
    }
}
void demo_corridor_enter(void){
    ec_game_init(&game,esp_random());
    screen=lv_obj_create(NULL);lv_obj_set_style_bg_color(screen,lv_color_hex(0x151818),0);
    lv_obj_set_style_pad_all(screen,0,0);lv_obj_set_style_border_width(screen,0,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    pixels=heap_caps_malloc(EC_IMAGE_BYTES,MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
    if(pixels)renderer=ec_renderer_create(ec_sprite_start,(size_t)(ec_sprite_end-ec_sprite_start));
    if(renderer){
        ec_renderer_set_clock(renderer,render_clock);
        image=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I8,.w=EC_WIDTH,.h=EC_HEIGHT,.stride=EC_WIDTH},.data_size=EC_IMAGE_BYTES,.data=pixels};
        ec_renderer_draw(renderer,&game,pixels);
        for(unsigned i=0;i<256;++i){const uint8_t *c=pixels+i*4;palette565[i]=(uint16_t)(((c[2]>>3)<<11)|((c[1]>>2)<<5)|(c[0]>>3));}
        image_decoder=lv_image_decoder_create();
        if(image_decoder){
        lv_image_decoder_set_info_cb(image_decoder,image_info);
        lv_image_decoder_set_open_cb(image_decoder,image_open);
        lv_image_decoder_set_get_area_cb(image_decoder,image_area);
        image_obj=lv_image_create(screen);lv_image_set_src(image_obj,&image);lv_obj_set_pos(image_obj,0,0);
        }else{ec_renderer_destroy(renderer);renderer=NULL;free(pixels);pixels=NULL;}
    }else{free(pixels);pixels=NULL;}
    bar=lv_obj_create(screen);lv_obj_remove_style_all(bar);lv_obj_set_size(bar,240,24);lv_obj_set_pos(bar,0,0);
    lv_obj_set_style_bg_color(bar,lv_color_hex(0x141918),0);lv_obj_set_style_bg_opa(bar,LV_OPA_80,0);
    lv_color_t paper=lv_color_hex(0xefeee8),muted=lv_color_hex(0xb6bdbd),gold=lv_color_hex(0xd6b954);
    status=label(bar,"",4,&corridor_font,paper);lv_obj_align(status,LV_ALIGN_TOP_LEFT,7,4);
    battery=label(bar,"--",4,&corridor_font,paper);lv_obj_align(battery,LV_ALIGN_TOP_RIGHT,-7,4);
    displayed_battery=-2;
    overlay=lv_obj_create(screen);lv_obj_remove_style_all(overlay);lv_obj_set_size(overlay,240,320);lv_obj_set_pos(overlay,0,0);
    lv_obj_set_style_bg_color(overlay,lv_color_hex(0x080d0d),0);lv_obj_set_style_bg_opa(overlay,LV_OPA_70,0);
    subtitle=label(overlay,"地下通道",66,&corridor_font,muted);
    title=label(overlay,"8号出口",94,&corridor_title_font,paper);
    rule=lv_obj_create(overlay);lv_obj_remove_style_all(rule);lv_obj_set_size(rule,32,2);lv_obj_set_pos(rule,104,145);
    lv_obj_set_style_bg_color(rule,gold,0);lv_obj_set_style_bg_opa(rule,LV_OPA_60,0);
    caption=label(overlay,"",166,&corridor_font,muted);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
    prompt=label(overlay,"",252,&corridor_font,gold);
    footer=label(overlay,"长按 OK 返回",287,&corridor_font,muted);
    lv_obj_move_foreground(bar);
    if(!renderer){
        lv_label_set_text(subtitle,"");lv_obj_set_style_text_font(title,&corridor_font,0);lv_label_set_text(title,"内存不足");
        lv_label_set_text(caption,"长按 OK 返回");lv_obj_add_flag(prompt,LV_OBJ_FLAG_HIDDEN);
    }
    prompt_opacity=LV_OPA_COVER;
    presentation_phase=-1;
    duel_io_activate(true);duel_io_sound(false,false,DUEL_CUE_NONE);
    visible=true;dirty=true;last_tick=esp_timer_get_time();last_render=last_log=last_tick;frame_count=0;render_total=0;
    tick_total=refresh_total=0;refresh_count=decoded_chunks=0;refresh_started=0;
    lv_display_add_event_cb(lv_display_get_default(),refresh_event,LV_EVENT_RENDER_START,NULL);
    lv_display_add_event_cb(lv_display_get_default(),refresh_event,LV_EVENT_RENDER_READY,NULL);
    hud();lv_screen_load(screen);
    ESP_LOGI(TAG,"ready=%d native=240x320 image=%u work=%u free=%u largest=%u",renderer!=NULL,(unsigned)EC_IMAGE_BYTES,(unsigned)ec_renderer_work_bytes(),(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL));
}
void demo_corridor_exit(void){
    visible=false;
    lv_display_remove_event_cb_with_user_data(lv_display_get_default(),refresh_event,NULL);
    duel_io_activate(false);
    if(image_obj)lv_image_cache_drop(&image);
    if(screen)lv_obj_delete(screen);
    if(image_decoder){lv_image_decoder_delete(image_decoder);image_decoder=NULL;}
    ec_renderer_destroy(renderer);free(pixels);
    renderer=NULL;pixels=NULL;screen=image_obj=status=battery=overlay=subtitle=title=caption=prompt=footer=rule=bar=NULL;
}
void demo_corridor_key(bsp_btn_t button,bsp_btn_ev_t event){
    if(!visible||!renderer)return;
    /* OK release avoids triggering a walk/restart before the global long exit. */
    if(button==BSP_BTN_OK&&event==BSP_BTN_CLICK)ec_game_key(&game,EC_OK);
    else if(button!=BSP_BTN_OK&&event==BSP_BTN_PRESS)ec_game_key(&game,button==BSP_BTN_UP?EC_LEFT:EC_RIGHT);
    else return;
    dirty=true;hud();
}
void demo_corridor_input_lost(void){if(visible){game.walking=false;game.observing=false;dirty=true;hud();}}
bool demo_corridor_return_to_title(void){
    if(!visible||!renderer)return false;
    ec_game_init(&game,esp_random());
    dirty=true;
    last_tick=esp_timer_get_time();
    last_render=0;
    hud();
    return true;
}
static void log_judgement(const char *kind){
    const ec_judgement_t *j=&game.last_judgement;
    ESP_LOGI(TAG,"%s passage=%u before=%u after=%u anomaly=%d entry_exit=%d crossed_exit=%d forward=%d correct=%d phase=%d",
        kind,game.passages,j->score_before,game.score,j->anomaly,j->entry_exit,j->crossed_exit,j->forward,j->correct,game.phase);
}
void demo_corridor_tick(int64_t now){
    if(!visible||!renderer)return;
    int64_t tick_started=esp_timer_get_time();
    update_battery();
    update_title_prompt(now);

    unsigned passages_before=game.passages;
    ec_phase_t phase_before=game.phase;
    ec_game_tick(&game,(now-last_tick)/1000000.0f);last_tick=now;
    /* Preserve the final redraw even after play stops or this tick is throttled. */
    if(game.phase!=phase_before)dirty=true;
    if(game.passages!=passages_before)log_judgement("judgement");
    tick_total+=(uint64_t)(esp_timer_get_time()-tick_started);
    if(now-last_render<50000)return;
    if(game.phase==EC_PLAYING||game.phase==EC_EXITING||dirty){
        int64_t start=esp_timer_get_time();ec_renderer_draw(renderer,&game,pixels);
        render_total+=(uint64_t)(esp_timer_get_time()-start);++frame_count;
        lv_image_cache_drop(&image);lv_obj_invalidate(image_obj);hud();dirty=false;
    }
    last_render=now;
    if(now-last_log>=5000000){
        ESP_LOGI(TAG,"frames=%u window_ms=%u render_avg_us=%u free=%u min=%u largest=%u phase=%d score=%u passages=%u",
            frame_count,(unsigned)((now-last_log)/1000),frame_count?(unsigned)(render_total/frame_count):0,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),game.phase,game.score,game.passages);
        uint32_t stages[3];ec_renderer_profile(renderer,stages);
        ESP_LOGI(TAG,"last_cpu_us floor=%u walls=%u sprite=%u",(unsigned)stages[0],(unsigned)stages[1],(unsigned)stages[2]);
        ESP_LOGI(TAG,"pipeline tick_us=%u refresh_avg_us=%u refreshes=%u decoded_chunks=%u",
            (unsigned)tick_total,refresh_count?(unsigned)(refresh_total/refresh_count):0,refresh_count,decoded_chunks);
        if(game.passages)log_judgement("last_judgement");
        ESP_LOGI(TAG,"view x_mm=%d z_mm=%d yaw_mrad=%d walking=%d turning=%d observing=%d",(int)(game.x*1000),(int)(game.z*1000),(int)(game.camera_yaw*1000),game.walking,game.turning,game.observing);
        frame_count=refresh_count=decoded_chunks=0;render_total=tick_total=refresh_total=0;last_log=now;
    }
}
