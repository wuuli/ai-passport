// main/demo.h —— 每个演示页实现的统一接口。
// 新增一个演示页 = 实现这三个函数 + 在 main.c 的 DEMOS[] 里加一行。
#pragma once

#include "bsp_button.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *name;
    void (*enter)(void);                          // 建自己的屏并载入
    void (*exit)(void);                           // 删屏、停定时器、释放资源
    void (*key)(bsp_btn_t btn, bsp_btn_ev_t ev);  // 收按键(长按确定由 main 分发或拦截)
    void (*key_at)(bsp_btn_t btn, bsp_btn_ev_t ev, int64_t timestamp_us);
    void (*tick)(int64_t now_us);
} demo_entry_t;

// 各演示页(定义在各自的 .c 里)
void demo_display_enter(void); void demo_display_exit(void);
void demo_display_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_button_enter(void);  void demo_button_exit(void);
void demo_button_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_audio_enter(void);   void demo_audio_exit(void);
void demo_audio_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_battery_enter(void); void demo_battery_exit(void);
void demo_battery_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_wifi_enter(void);    void demo_wifi_exit(void);
void demo_wifi_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_ble_enter(void);     void demo_ble_exit(void);
void demo_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_low_power_enter(void); void demo_low_power_exit(void);
void demo_low_power_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_duel_enter(void);
void demo_duel_exit(void);
void demo_duel_key(bsp_btn_t button, bsp_btn_ev_t event);
void demo_duel_key_at(bsp_btn_t button, bsp_btn_ev_t event, int64_t timestamp_us);
void demo_duel_tick(int64_t now_us);
void demo_duel_input_lost(int64_t now_us);

void demo_corridor_enter(void);
void demo_corridor_exit(void);
void demo_corridor_key(bsp_btn_t button, bsp_btn_ev_t event);
void demo_corridor_tick(int64_t now_us);
void demo_corridor_input_lost(void);
bool demo_corridor_return_to_title(void);
