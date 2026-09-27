#include "game_launcher.h"
#include "lvgl.h"
#include <stdio.h>

extern const lv_font_t corridor_title_font;
extern const lv_font_t corridor_font;
extern const lv_font_t duel_title_font;
extern const lv_font_t duel_font;

#define COLOR_BG          0x101314
#define COLOR_TEXT_MUTED  0x8A9696
#define COLOR_TEXT_DIM    0x4E5C5C
#define COLOR_HINT_BG     0x171C1D
#define COLOR_HINT_BORDER 0x242D2E

// Exit 8 theme
#define CORRIDOR_ACCENT       0x00E676
#define CORRIDOR_BG_ACTIVE    0x152822
#define CORRIDOR_BG_IDLE      0x131A18
#define CORRIDOR_BORDER_IDLE  0x24302C
#define CORRIDOR_TEXT_ACTIVE  0xFFFFFF
#define CORRIDOR_TEXT_IDLE    0x788585
#define CORRIDOR_SUB_IDLE     0x4A6B60
#define CORRIDOR_DESC_ACTIVE  0x8CD9B3
#define CORRIDOR_DESC_IDLE    0x3E544C

// Time Challenge theme
#define DUEL_GOLD_ACCENT      0xCBA65E
#define DUEL_PAPER_TEXT       0xF1DCA4
#define DUEL_BG_ACTIVE        0x242016
#define DUEL_BG_IDLE          0x161512
#define DUEL_BORDER_IDLE      0x302B20
#define DUEL_TEXT_IDLE        0x7C7462
#define DUEL_SUB_IDLE         0x61563F
#define DUEL_DESC_ACTIVE      0xD6C18E
#define DUEL_DESC_IDLE        0x4A4232

static lv_obj_t *s_screen;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_card_corridor;
static lv_obj_t *s_card_duel;
static lv_obj_t *s_title_corridor;
static lv_obj_t *s_title_duel;
static lv_obj_t *s_sub_corridor;
static lv_obj_t *s_sub_duel;
static lv_obj_t *s_desc_corridor;
static lv_obj_t *s_desc_duel;
static lv_obj_t *s_tag_corridor;
static lv_obj_t *s_tag_duel;
static lv_obj_t *s_hint_label;

static unsigned s_cached_selected;
static int s_cached_battery = -2;

static lv_obj_t *create_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg_color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y, int w,
                              const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    if (w > 0) {
        lv_obj_set_width(obj, w);
    }
    if (font) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, text ? text : "");
    return obj;
}

static void apply_selection(unsigned selected)
{
    const bool sel_corridor = (selected == 0);
    const bool sel_duel = (selected == 1);

    if (s_card_corridor) {
        lv_obj_set_style_bg_color(s_card_corridor,
            lv_color_hex(sel_corridor ? CORRIDOR_BG_ACTIVE : CORRIDOR_BG_IDLE), 0);
        lv_obj_set_style_border_color(s_card_corridor,
            lv_color_hex(sel_corridor ? CORRIDOR_ACCENT : CORRIDOR_BORDER_IDLE), 0);
        lv_obj_set_style_border_width(s_card_corridor, sel_corridor ? 2 : 1, 0);
    }
    if (s_title_corridor) {
        lv_obj_set_style_text_color(s_title_corridor,
            lv_color_hex(sel_corridor ? CORRIDOR_TEXT_ACTIVE : CORRIDOR_TEXT_IDLE), 0);
    }
    if (s_sub_corridor) {
        lv_obj_set_style_text_color(s_sub_corridor,
            lv_color_hex(sel_corridor ? CORRIDOR_ACCENT : CORRIDOR_SUB_IDLE), 0);
    }
    if (s_desc_corridor) {
        lv_obj_set_style_text_color(s_desc_corridor,
            lv_color_hex(sel_corridor ? CORRIDOR_DESC_ACTIVE : CORRIDOR_DESC_IDLE), 0);
    }
    if (s_tag_corridor) {
        lv_label_set_text(s_tag_corridor, sel_corridor ? "[ READY ]" : "");
    }

    if (s_card_duel) {
        lv_obj_set_style_bg_color(s_card_duel,
            lv_color_hex(sel_duel ? DUEL_BG_ACTIVE : DUEL_BG_IDLE), 0);
        lv_obj_set_style_border_color(s_card_duel,
            lv_color_hex(sel_duel ? DUEL_GOLD_ACCENT : DUEL_BORDER_IDLE), 0);
        lv_obj_set_style_border_width(s_card_duel, sel_duel ? 2 : 1, 0);
    }
    if (s_title_duel) {
        lv_obj_set_style_text_color(s_title_duel,
            lv_color_hex(sel_duel ? DUEL_PAPER_TEXT : DUEL_TEXT_IDLE), 0);
    }
    if (s_sub_duel) {
        lv_obj_set_style_text_color(s_sub_duel,
            lv_color_hex(sel_duel ? DUEL_GOLD_ACCENT : DUEL_SUB_IDLE), 0);
    }
    if (s_desc_duel) {
        lv_obj_set_style_text_color(s_desc_duel,
            lv_color_hex(sel_duel ? DUEL_DESC_ACTIVE : DUEL_DESC_IDLE), 0);
    }
    if (s_tag_duel) {
        lv_label_set_text(s_tag_duel, sel_duel ? "[ READY ]" : "");
    }
}

static void apply_battery(int battery)
{
    if (!s_battery_label) return;
    if (battery >= 0) {
        lv_label_set_text_fmt(s_battery_label, "%d%%", battery);
    } else {
        lv_label_set_text(s_battery_label, "--%");
    }
}

bool game_launcher_create(unsigned selected, int battery)
{
    lv_obj_t *old_screen = s_screen;

    lv_obj_t *screen = lv_obj_create(NULL);
    if (!screen) {
        return false;
    }

    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // Header bar (y = 8): Left "GAMES", Right battery
    create_label(screen, "GAMES", 14, 8, 80, &lv_font_montserrat_14, COLOR_TEXT_MUTED);

    s_battery_label = create_label(screen, "--%", 174, 8, 52, &lv_font_montserrat_14, COLOR_TEXT_MUTED);
    lv_obj_set_style_text_align(s_battery_label, LV_TEXT_ALIGN_RIGHT, 0);

    // Card 0: 8号出口 / EXIT 8 (y = 30, h = 118)
    s_card_corridor = create_box(screen, 12, 30, 216, 118, CORRIDOR_BG_IDLE);
    lv_obj_set_style_radius(s_card_corridor, 6, 0);
    lv_obj_set_style_border_width(s_card_corridor, 1, 0);
    lv_obj_set_style_border_color(s_card_corridor, lv_color_hex(CORRIDOR_BORDER_IDLE), 0);

    s_title_corridor = create_label(s_card_corridor, "8号出口", 14, 12, 188,
                                    &corridor_title_font, CORRIDOR_TEXT_IDLE);
    s_sub_corridor = create_label(s_card_corridor, "EXIT 8", 14, 46, 188,
                                  &lv_font_montserrat_14, CORRIDOR_SUB_IDLE);
    s_desc_corridor = create_label(s_card_corridor, "地下通道", 14, 68, 188,
                                   &corridor_font, CORRIDOR_DESC_IDLE);
    s_tag_corridor = create_label(s_card_corridor, "", 14, 92, 188,
                                  &lv_font_montserrat_14, CORRIDOR_ACCENT);

    // Card 1: 掐秒挑战 / TIME CHALLENGE (y = 156, h = 118)
    s_card_duel = create_box(screen, 12, 156, 216, 118, DUEL_BG_IDLE);
    lv_obj_set_style_radius(s_card_duel, 6, 0);
    lv_obj_set_style_border_width(s_card_duel, 1, 0);
    lv_obj_set_style_border_color(s_card_duel, lv_color_hex(DUEL_BORDER_IDLE), 0);

    s_title_duel = create_label(s_card_duel, "掐秒挑战", 14, 12, 188,
                                &duel_title_font, DUEL_TEXT_IDLE);
    s_sub_duel = create_label(s_card_duel, "TIME CHALLENGE", 14, 46, 188,
                              &lv_font_montserrat_14, DUEL_SUB_IDLE);
    s_desc_duel = create_label(s_card_duel, "精准掐秒 · 双人对抗", 14, 68, 188,
                               &duel_font, DUEL_DESC_IDLE);
    s_tag_duel = create_label(s_card_duel, "", 14, 92, 188,
                              &lv_font_montserrat_14, DUEL_GOLD_ACCENT);

    // Bottom Hint Box (y = 282, h = 30)
    lv_obj_t *hint_box = create_box(screen, 12, 282, 216, 30, COLOR_HINT_BG);
    lv_obj_set_style_radius(hint_box, 4, 0);
    lv_obj_set_style_border_width(hint_box, 1, 0);
    lv_obj_set_style_border_color(hint_box, lv_color_hex(COLOR_HINT_BORDER), 0);

    static lv_font_t s_hint_font;
    static bool s_hint_font_inited;
    if (!s_hint_font_inited) {
        s_hint_font = lv_font_montserrat_14;
        s_hint_font.fallback = &duel_font;
        s_hint_font_inited = true;
    }

    s_hint_label = create_label(hint_box, "UP/DOWN select · OK enter", 0, 7, 216,
                                &s_hint_font, COLOR_TEXT_MUTED);
    lv_obj_set_style_text_align(s_hint_label, LV_TEXT_ALIGN_CENTER, 0);

    s_cached_selected = selected;
    s_cached_battery = (battery >= 0 && battery <= 100) ? battery : -1;
    apply_selection(s_cached_selected);
    apply_battery(s_cached_battery);

    // Safe load: activate new screen first, then release the old screen.
    lv_screen_load(screen);
    if (old_screen && old_screen != screen) {
        lv_obj_delete(old_screen);
    }
    s_screen = screen;
    return true;
}

void game_launcher_update(unsigned selected, int battery)
{
    if (!s_screen) return;

    const int norm_battery = (battery >= 0 && battery <= 100) ? battery : -1;

    // Cache guard: avoid invalidating or updating when state is identical
    if (selected == s_cached_selected && norm_battery == s_cached_battery) {
        return;
    }

    if (norm_battery != s_cached_battery) {
        s_cached_battery = norm_battery;
        apply_battery(s_cached_battery);
    }

    if (selected != s_cached_selected) {
        s_cached_selected = selected;
        apply_selection(s_cached_selected);
    }
}

void game_launcher_destroy(void)
{
    if (s_screen) {
        lv_obj_t *screen = s_screen;
        s_screen = NULL;
        s_battery_label = NULL;
        s_card_corridor = NULL;
        s_card_duel = NULL;
        s_title_corridor = NULL;
        s_title_duel = NULL;
        s_sub_corridor = NULL;
        s_sub_duel = NULL;
        s_desc_corridor = NULL;
        s_desc_duel = NULL;
        s_tag_corridor = NULL;
        s_tag_duel = NULL;
        s_hint_label = NULL;
        s_cached_selected = 0;
        s_cached_battery = -2;
        lv_obj_delete(screen);
    }
}
