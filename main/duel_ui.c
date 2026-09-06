#include "duel_ui.h"

#include "duel_assets.h"
#include "lvgl.h"
#include <stdio.h>

#define DUEL_INK 0x11160F
#define DUEL_OLIVE 0x252A20
#define DUEL_PAPER 0xF1DCA4
#define DUEL_GOLD 0xCBA65E

static lv_obj_t *s_screen;
static lv_obj_t *s_body;
static lv_obj_t *s_status;
static lv_obj_t *s_battery;
static int s_battery_value = -1;
static lv_obj_t *s_hint;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width,
                        const lv_font_t *font, uint32_t color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(object, text);
    return object;
}

static void text(const char *copy, int y)
{
    label(s_body, copy, 8, y, 224, &duel_font, DUEL_PAPER);
}

static void target_panel(uint32_t target_ms)
{
    lv_obj_t *target = box(s_body, 24, 71, 192, 105, DUEL_INK);
    lv_obj_set_style_border_color(target, lv_color_hex(DUEL_GOLD), 0);
    lv_obj_set_style_border_width(target, 2, 0);
    label(target, "目标时长", 4, 8, 184, &duel_font, DUEL_PAPER);
    char copy[32];
    snprintf(copy, sizeof(copy), "%u.%u s", (unsigned)(target_ms / 1000),
             (unsigned)(target_ms % 1000 / 100));
    label(target, copy, 4, 38, 184, &lv_font_montserrat_40, DUEL_GOLD);
}

static const char *player_name(const duel_view_t *view, unsigned player)
{
    return player == 0 ? "特工 01" : view->mode == DUEL_MODE_AI ? "AI 教官" : "特工 02";
}

static lv_obj_t *agent(unsigned player, bool cheering, int x, int y)
{
    lv_obj_t *object = lv_image_create(s_body);
    const lv_image_dsc_t *poses[2][2] = {
        {&duel_agent_0, &duel_agent_0_win},
        {&duel_agent_1, &duel_agent_1_win},
    };
    lv_image_set_src(object, poses[player][cheering ? 1 : 0]);
    lv_obj_set_pos(object, x, y);
    return object;
}

static void duo(int y)
{
    agent(0, false, 18, y);
    agent(1, false, 142, y);
    label(s_body, "VS", 100, y + 46, 40, &lv_font_montserrat_14, DUEL_GOLD);
}

static void move_y(void *object, int32_t y)
{
    lv_obj_set_y(object, y);
}

static void celebrate(lv_obj_t *object, int y)
{
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_exec_cb(&animation, move_y);
    lv_anim_set_values(&animation, y, y - 8);
    lv_anim_set_duration(&animation, 170);
    lv_anim_set_playback_duration(&animation, 170);
    lv_anim_set_repeat_count(&animation, 2);
    lv_anim_start(&animation);
}

bool duel_ui_create(void)
{
    if (s_screen) return true;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(DUEL_OLIVE), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_t *background = lv_image_create(s_screen);
    lv_image_set_src(background, &duel_outpost);
    lv_obj_set_pos(background, 0, 28);
    lv_obj_t *shade = box(s_screen, 0, 28, 240, 240, DUEL_OLIVE);
    lv_obj_set_style_bg_opa(shade, LV_OPA_70, 0);
    box(s_screen, 0, 0, 240, 28, DUEL_INK);
    s_status = label(s_screen, "", 5, 5, 180, &duel_font, DUEL_PAPER);
    s_battery = label(s_screen, "--%", 185, 7, 50, &lv_font_montserrat_14, DUEL_PAPER);
    s_battery_value = -1;
    s_body = box(s_screen, 0, 31, 240, 235, DUEL_OLIVE);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_TRANSP, 0);
    lv_obj_t *hint_panel = box(s_screen, 12, 274, 216, 28, DUEL_GOLD);
    s_hint = label(hint_panel, "", 2, 4, 212, &duel_font, DUEL_INK);
    label(s_screen, "长按 OK 返回菜单", 8, 300, 224, &duel_font, DUEL_PAPER);
    lv_screen_load(s_screen);
    return true;
}

void duel_ui_set_battery(int battery)
{
    if (!s_battery) return;
    const int value = battery >= 0 && battery <= 100 ? battery : -1;
    if (value == s_battery_value) return;
    s_battery_value = value;
    if (value >= 0) lv_label_set_text_fmt(s_battery, "%d%%", value);
    else lv_label_set_text(s_battery, "--%");
}

void duel_ui_render(const duel_view_t *view, int battery, bool sound, bool audio_ready,
                    bool input_lost)
{
    if (!s_screen) return;
    lv_obj_clean(s_body);
    char copy[128];
    duel_ui_set_battery(battery);
    if (view->phase == DUEL_PHASE_HOME) lv_label_set_text(s_status, "时间感训练");
    else lv_label_set_text_fmt(s_status, "第 %u 回合  %u : %u", (unsigned)view->round,
                               (unsigned)view->score[0], (unsigned)view->score[1]);
    const char *hint = "";
    switch (view->phase) {
    case DUEL_PHASE_HOME:
        label(s_body, "掐秒挑战", 8, 0, 224, &duel_title_font, DUEL_GOLD);
        text(input_lost ? "输入丢失，请重新开始" : "不比手快，比时间感", 34);
        duo(60);
        text(view->mode == DUEL_MODE_DUO ? "双人对抗 · 五局三胜" : "单人练习 · AI 教官", 176);
        snprintf(copy, sizeof(copy), "UP 模式  DOWN %s", !audio_ready ? "静音" : sound ? "声音开" : "声音关");
        text(copy, 209);
        hint = "OK 开始训练";
        break;
    case DUEL_PHASE_TARGET: {
        snprintf(copy, sizeof(copy), "%s 的回合", player_name(view, view->current));
        text(copy, 7);
        text("时间感训练", 35);
        target_panel(view->target_ms);
        text("记住时长，凭感觉掐秒", 193);
        hint = "OK 开始计时";
        break;
    }
    case DUEL_PHASE_TIMING:
        snprintf(copy, sizeof(copy), "%s 的回合", player_name(view, view->current));
        text(copy, 8);
        agent(view->current, false, 80, 47);
        label(s_body, "TRUST YOUR GUT", 8, 172, 224, &lv_font_montserrat_14, DUEL_GOLD);
        text("感觉时间到了，就按 OK", 203);
        hint = "OK 停止计时";
        break;
    case DUEL_PHASE_HANDOVER:
        snprintf(copy, sizeof(copy), "%s，轮到你", player_name(view, view->current));
        text(copy, 7);
        text("训练成绩暂不公开", 35);
        target_panel(view->target_ms);
        text(view->automatic[1 - view->current] ? "上一位已自动停止" : "接过终端，按 OK 开始", 193);
        hint = "OK 开始计时";
        break;
    case DUEL_PHASE_AI_WAIT:
        agent(1, false, 80, 35);
        text("教官正在掐秒", 156);
        text("你的成绩已锁定", 191);
        hint = "等待教官完成";
        break;
    case DUEL_PHASE_SEALED:
        duo(24);
        text("谁的时间感更准？", 153);
        text("双方成绩已锁定", 188);
        hint = "OK 揭晓本轮结果";
        break;
    case DUEL_PHASE_ROUND_END:
        label(s_body, view->winner < 0 ? "DRAW" : "ROUND WIN", 5, 0, 230,
              &lv_font_montserrat_20, DUEL_GOLD);
        if (view->winner < 0) {
            text("一样精准，本轮重赛", 28);
            duo(57);
        } else {
            snprintf(copy, sizeof(copy), "%s 赢下本轮", player_name(view, (unsigned)view->winner));
            text(copy, 28);
            celebrate(agent((unsigned)view->winner, true, 80, 58), 58);
        }
        snprintf(copy, sizeof(copy), "%u : %u", (unsigned)view->score[0], (unsigned)view->score[1]);
        label(s_body, copy, 8, 177, 224, &lv_font_montserrat_40, DUEL_GOLD);
        hint = "OK 跳过动画";
        break;
    case DUEL_PHASE_RESULT:
        snprintf(copy, sizeof(copy), view->winner < 0 ? "误差相同，重赛本轮" : "%s 更精准",
                 player_name(view, view->winner < 0 ? 0 : (unsigned)view->winner));
        text(copy, 2);
        snprintf(copy, sizeof(copy), "目标 %u.%u 秒", (unsigned)(view->target_ms / 1000),
                 (unsigned)(view->target_ms % 1000 / 100));
        text(copy, 30);
        for (unsigned player = 0; player < 2; ++player) {
            lv_obj_t *card = box(s_body, player == 0 ? 10 : 126, 61, 104, 120, DUEL_INK);
            const uint32_t color = player == 0 ? 0xCCA266 : 0x8FAEB7;
            label(card, player_name(view, player), 2, 6, 100, &duel_font, color);
            snprintf(copy, sizeof(copy), "%u.%03u", (unsigned)(view->elapsed_ms[player] / 1000),
                     (unsigned)(view->elapsed_ms[player] % 1000));
            label(card, copy, 2, 31, 100, &lv_font_montserrat_20, DUEL_PAPER);
            snprintf(copy, sizeof(copy), "%u ms", (unsigned)view->error_ms[player]);
            label(card, "误差", 2, 58, 100, &duel_font, color);
            label(card, copy, 2, 81, 100, &lv_font_montserrat_14, DUEL_PAPER);
            if (view->automatic[player]) label(card, "自动停止", 2, 100, 100, &duel_font, color);
        }
        snprintf(copy, sizeof(copy), "训练战绩  %u : %u", (unsigned)view->score[0], (unsigned)view->score[1]);
        text(copy, 197);
        hint = view->score[0] == 3 || view->score[1] == 3 ? "OK 查看整场结果" :
               view->winner < 0 ? "OK 重赛这一局" : "OK 下一回合";
        break;
    case DUEL_PHASE_MATCH_WIN:
        lv_label_set_text(s_status, "训练结束");
        label(s_body, "训练优胜", 8, 3, 224, &duel_title_font, DUEL_GOLD);
        snprintf(copy, sizeof(copy), "%s 赢得本场对抗", player_name(view, (unsigned)view->winner));
        text(copy, 38);
        celebrate(agent((unsigned)view->winner, true, 80, 75), 75);
        label(s_body, "*", 32, 86, 40, &lv_font_montserrat_40, DUEL_GOLD);
        label(s_body, "*", 167, 117, 40, &lv_font_montserrat_40, DUEL_GOLD);
        snprintf(copy, sizeof(copy), "%u : %u", (unsigned)view->score[0], (unsigned)view->score[1]);
        label(s_body, copy, 8, 189, 224, &lv_font_montserrat_40, DUEL_GOLD);
        hint = "OK 返回首页";
        break;
    }
    lv_label_set_text(s_hint, hint);
}

void duel_ui_destroy(void)
{
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
    s_body = NULL;
    s_status = NULL;
    s_battery = NULL;
    s_hint = NULL;
}
