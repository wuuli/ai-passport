#pragma once
#include "corridor_game.h"
#include <stddef.h>
#include <stdint.h>

enum { EC_WIDTH = 240, EC_HEIGHT = 320, EC_PALETTE_BYTES = 1024,
       EC_IMAGE_BYTES = EC_PALETTE_BYTES + EC_WIDTH * EC_HEIGHT };
typedef struct ec_renderer ec_renderer_t;
/* Allocates bounded working state. Sprite bytes remain caller-owned in flash. */
ec_renderer_t *ec_renderer_create(const uint8_t *sprite, size_t sprite_size);
void ec_renderer_destroy(ec_renderer_t *renderer);
/* Writes LVGL I8 palette (ARGB8888) followed by full native 240x320 pixels. */
void ec_renderer_draw(ec_renderer_t *renderer, const ec_game_t *game, uint8_t *image);
size_t ec_renderer_work_bytes(void);

/* Optional monotonic microsecond clock; timings are CPU stages, not LCD FPS. */
void ec_renderer_set_clock(ec_renderer_t *renderer, uint32_t (*clock_us)(void));
void ec_renderer_profile(const ec_renderer_t *renderer, uint32_t stages_us[3]);
