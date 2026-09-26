#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define LV_IMAGE_HEADER_MAGIC 0x1234
#define LV_COORD_MIN (-32768)
#define LV_OPA_TRANSP 0
#define LV_OPA_80 204
#define LV_OPA_90 230
#define LV_OPA_70 178
#define LV_OPA_60 153
#define LV_OPA_30 76
#define LV_OPA_COVER 255

typedef enum {
    LV_RESULT_INVALID = 0,
    LV_RESULT_OK = 1,
} lv_result_t;

typedef enum {
    LV_COLOR_FORMAT_I8 = 1,
    LV_COLOR_FORMAT_RGB565 = 2,
} lv_color_format_t;

typedef enum {
    LV_EVENT_RENDER_START = 1,
    LV_EVENT_RENDER_READY = 2,
} lv_event_code_t;

typedef enum {
    LV_ALIGN_TOP_MID = 1,
    LV_ALIGN_TOP_LEFT = 2,
    LV_ALIGN_TOP_RIGHT = 3,
} lv_align_t;

typedef enum {
    LV_TEXT_ALIGN_CENTER = 1,
} lv_text_align_t;

enum {
    LV_OBJ_FLAG_HIDDEN = (1 << 0),
    LV_OBJ_FLAG_SCROLLABLE = (1 << 1),
};
typedef uint32_t lv_obj_flag_t;

typedef uint32_t lv_style_selector_t;
typedef uint32_t lv_color_t;
typedef uint8_t lv_opa_t;

typedef struct {
    int32_t x1, y1, x2, y2;
} lv_area_t;

typedef struct {
    uint32_t magic;
    lv_color_format_t cf;
    uint32_t w;
    uint32_t h;
    uint32_t stride;
} lv_image_header_t;

typedef struct {
    lv_image_header_t header;
    uint32_t data_size;
    const uint8_t *data;
} lv_image_dsc_t;

typedef struct lv_draw_buf_s {
    uint32_t width;
    uint32_t height;
    lv_color_format_t cf;
    uint32_t stride;
    void *data;
    size_t data_size;
} lv_draw_buf_t;

typedef struct lv_font_s {
    int dummy;
} lv_font_t;

typedef struct lv_event_s {
    lv_event_code_t code;
} lv_event_t;

typedef void (*lv_event_cb_t)(lv_event_t *e);

typedef struct lv_display_s {
    int dummy;
} lv_display_t;

typedef struct lv_image_decoder_s lv_image_decoder_t;

typedef struct {
    const void *src;
    const lv_draw_buf_t *decoded;
    struct {
        bool no_cache;
    } args;
} lv_image_decoder_dsc_t;

typedef lv_result_t (*lv_image_decoder_info_cb_t)(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc, lv_image_header_t *header);
typedef lv_result_t (*lv_image_decoder_open_cb_t)(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc);
typedef lv_result_t (*lv_image_decoder_get_area_cb_t)(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc, const lv_area_t *full, lv_area_t *area);

struct lv_image_decoder_s {
    lv_image_decoder_info_cb_t info_cb;
    lv_image_decoder_open_cb_t open_cb;
    lv_image_decoder_get_area_cb_t get_area_cb;
};

struct lv_obj_s {
    uint32_t flags;
    char text[256];
    int32_t x, y, w, h;
    lv_color_t bg_color, text_color;
    lv_opa_t bg_opa, text_opa;
    const lv_font_t *font;
    struct lv_obj_s *parent;
    struct lv_obj_s *next;
    struct lv_obj_s *children;
};
typedef struct lv_obj_s lv_obj_t;

static int s_lv_obj_alive_count;

static inline int lv_obj_get_alive_count(void) {
    return s_lv_obj_alive_count;
}

static inline lv_color_t lv_color_hex(uint32_t c) { return c; }

static inline lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    lv_obj_t *obj = (lv_obj_t *)calloc(1, sizeof(lv_obj_t));
    if (obj) {
        s_lv_obj_alive_count++;
        obj->parent = parent;
        if (parent) {
            obj->next = parent->children;
            parent->children = obj;
        }
    }
    return obj;
}

static inline void lv_obj_delete(lv_obj_t *obj) {
    if (!obj) return;
    lv_obj_t *child = obj->children;
    while (child) {
        lv_obj_t *next = child->next;
        child->parent = NULL;
        lv_obj_delete(child);
        child = next;
    }
    obj->children = NULL;

    if (obj->parent) {
        lv_obj_t **curr = &obj->parent->children;
        while (*curr) {
            if (*curr == obj) {
                *curr = obj->next;
                break;
            }
            curr = &(*curr)->next;
        }
        obj->parent = NULL;
    }
    s_lv_obj_alive_count--;
    free(obj);
}

static inline void lv_obj_add_flag(lv_obj_t *obj, uint32_t flag) {
    if (obj) obj->flags |= flag;
}

static inline void lv_obj_remove_flag(lv_obj_t *obj, uint32_t flag) {
    if (obj) obj->flags &= ~flag;
}

static inline bool lv_obj_has_flag(const lv_obj_t *obj, uint32_t flag) {
    return obj ? ((obj->flags & flag) != 0) : false;
}

static inline void lv_obj_remove_style_all(lv_obj_t *obj) { (void)obj; }
static inline void lv_obj_set_size(lv_obj_t *obj, int32_t w, int32_t h) { if (obj) { obj->w = w; obj->h = h; } }
static inline void lv_obj_set_pos(lv_obj_t *obj, int32_t x, int32_t y) { if (obj) { obj->x = x; obj->y = y; } }
static inline void lv_obj_center(lv_obj_t *obj) { (void)obj; }
static inline void lv_obj_align(lv_obj_t *obj, lv_align_t a, int32_t x, int32_t y) { (void)a; if (obj) { obj->x=x; obj->y=y; } }
static inline void lv_obj_move_foreground(lv_obj_t *obj) { (void)obj; }
static inline void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t v, lv_style_selector_t s) { (void)s; if (obj) obj->bg_color=v; }
static inline void lv_obj_set_style_pad_all(lv_obj_t *obj, int32_t v, lv_style_selector_t s) { (void)obj; (void)v; (void)s; }
static inline void lv_obj_set_style_border_width(lv_obj_t *obj, int32_t v, lv_style_selector_t s) { (void)obj; (void)v; (void)s; }
static inline void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t v, lv_style_selector_t s) { (void)obj; (void)v; (void)s; }
static inline void lv_obj_set_style_bg_opa(lv_obj_t *obj, lv_opa_t v, lv_style_selector_t s) { (void)s; if (obj) obj->bg_opa=v; }
static inline void lv_obj_set_style_opa(lv_obj_t *obj, lv_opa_t v, lv_style_selector_t s) { (void)obj; (void)v; (void)s; }
static inline void lv_obj_set_style_text_opa(lv_obj_t *obj, lv_opa_t v, lv_style_selector_t s) { (void)s; if (obj) obj->text_opa=v; }
static inline void lv_obj_set_style_text_font(lv_obj_t *obj, const lv_font_t *f, lv_style_selector_t s) { (void)s; if (obj) obj->font=f; }
static inline void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t c, lv_style_selector_t s) { (void)s; if (obj) obj->text_color=c; }
static inline void lv_obj_set_style_text_align(lv_obj_t *obj, lv_text_align_t a, lv_style_selector_t s) { (void)obj; (void)a; (void)s; }
static inline void lv_obj_invalidate(const lv_obj_t *obj) { (void)obj; }
static inline void lv_screen_load(lv_obj_t *scr) { (void)scr; }

static inline lv_obj_t *lv_label_create(lv_obj_t *parent) {
    return lv_obj_create(parent);
}

static inline void lv_label_set_text(lv_obj_t *obj, const char *text) {
    if (obj) {
        snprintf(obj->text, sizeof(obj->text), "%s", text ? text : "");
    }
}

static inline void lv_label_set_text_fmt(lv_obj_t *obj, const char *fmt, ...) {
    if (obj) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(obj->text, sizeof(obj->text), fmt, args);
        va_end(args);
    }
}

static inline lv_obj_t *lv_image_create(lv_obj_t *parent) {
    return lv_obj_create(parent);
}

static inline void lv_image_set_src(lv_obj_t *obj, const void *src) { (void)obj; (void)src; }

static inline lv_image_decoder_t *lv_image_decoder_create(void) {
    return (lv_image_decoder_t *)calloc(1, sizeof(lv_image_decoder_t));
}

static inline lv_result_t lv_image_decoder_delete(lv_image_decoder_t *decoder) {
    free(decoder);
    return LV_RESULT_OK;
}

static inline void lv_image_decoder_set_info_cb(lv_image_decoder_t *d, lv_image_decoder_info_cb_t cb) {
    if (d) d->info_cb = cb;
}

static inline void lv_image_decoder_set_open_cb(lv_image_decoder_t *d, lv_image_decoder_open_cb_t cb) {
    if (d) d->open_cb = cb;
}

static inline void lv_image_decoder_set_get_area_cb(lv_image_decoder_t *d, lv_image_decoder_get_area_cb_t cb) {
    if (d) d->get_area_cb = cb;
}

static inline lv_display_t *lv_display_get_default(void) {
    static lv_display_t s_disp;
    return &s_disp;
}

static inline void lv_display_add_event_cb(lv_display_t *disp, lv_event_cb_t cb, lv_event_code_t filter, void *user_data) {
    (void)disp; (void)cb; (void)filter; (void)user_data;
}

static inline void lv_display_remove_event_cb_with_user_data(lv_display_t *disp, lv_event_cb_t cb, void *user_data) {
    (void)disp; (void)cb; (void)user_data;
}

static inline lv_event_code_t lv_event_get_code(lv_event_t *event) {
    return event ? event->code : (lv_event_code_t)0;
}

static inline lv_result_t lv_draw_buf_init(lv_draw_buf_t *draw_buf, uint32_t w, uint32_t h,
                                           lv_color_format_t cf, uint32_t stride, void *data, size_t data_size) {
    if (draw_buf) {
        draw_buf->width = w;
        draw_buf->height = h;
        draw_buf->cf = cf;
        draw_buf->stride = stride;
        draw_buf->data = data;
        draw_buf->data_size = data_size;
    }
    return LV_RESULT_OK;
}

static inline void lv_image_cache_drop(const void *src) { (void)src; }
