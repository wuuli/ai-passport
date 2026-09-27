#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct lv_timer_t lv_timer_t;
typedef void (*lv_timer_cb_t)(lv_timer_t *);
struct lv_timer_t {
    lv_timer_cb_t timer_cb;
    uint32_t period;
    void *user_data;
};

lv_timer_t *lv_timer_create(lv_timer_cb_t timer_xcb, uint32_t period, void *user_data);
