#include "duel_ui.h"
#include "lvgl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t s_frame[240 * 320];
static uint16_t s_draw[240 * 20];
static unsigned s_flushes;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    const uint16_t *source = (const uint16_t *)pixels;
    ++s_flushes;
    for (int row = area->y1; row <= area->y2; ++row) {
        for (int column = area->x1; column <= area->x2; ++column) {
            assert(row >= 0 && row < 320 && column >= 0 && column < 240);
            s_frame[row * 240 + column] = *source++;
        }
    }
    lv_display_flush_ready(display);
}

static void check_labels(lv_obj_t *object)
{
    if (lv_obj_check_type(object, &lv_label_class)) {
        lv_area_t bounds;
        lv_obj_get_coords(object, &bounds);
        if (bounds.x1 < 0 || bounds.x2 >= 240 || bounds.y1 < 0 || bounds.y2 >= 320) {
            fprintf(stderr, "Clipped label: %s at %d,%d..%d,%d\n", lv_label_get_text(object),
                    bounds.x1, bounds.y1, bounds.x2, bounds.y2);
            abort();
        }
        const unsigned char *cursor = (const unsigned char *)lv_label_get_text(object);
        const lv_font_t *font = lv_obj_get_style_text_font(object, 0);
        while (*cursor) {
            uint32_t codepoint = *cursor++;
            unsigned trailing = 0;
            if (codepoint >= 0xf0) { trailing = 3; codepoint &= 7; }
            else if (codepoint >= 0xe0) { trailing = 2; codepoint &= 15; }
            else if (codepoint >= 0xc0) { trailing = 1; codepoint &= 31; }
            while (trailing--) { assert(*cursor); codepoint = (codepoint << 6) | (*cursor++ & 63); }
            if (codepoint == '\n') continue;
            lv_font_glyph_dsc_t glyph;
            if (!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0) || glyph.is_placeholder) {
                fprintf(stderr, "Missing glyph U+%04X in %s\n", (unsigned)codepoint, lv_label_get_text(object));
                abort();
            }
        }
    }
    for (uint32_t child = 0; child < lv_obj_get_child_count(object); ++child) {
        check_labels(lv_obj_get_child(object, (int32_t)child));
    }
}

static unsigned collect_objects(lv_obj_t *object, lv_obj_t **objects, unsigned count)
{
    assert(count < 64);
    objects[count++] = object;
    for (uint32_t child = 0; child < lv_obj_get_child_count(object); ++child) {
        count = collect_objects(lv_obj_get_child(object, (int32_t)child), objects, count);
    }
    return count;
}

static lv_obj_t *find_label(const char *copy)
{
    lv_obj_t *objects[64];
    const unsigned count = collect_objects(lv_screen_active(), objects, 0);
    for (unsigned index = 0; index < count; ++index) {
        if (lv_obj_check_type(objects[index], &lv_label_class) &&
            strcmp(lv_label_get_text(objects[index]), copy) == 0) return objects[index];
    }
    return NULL;
}

static void check_target_handover_parity(lv_display_t *display)
{
    lv_obj_t *fallback = lv_screen_active();
    assert(duel_ui_create());
    for (unsigned target_index = 0; target_index < DUEL_TARGET_COUNT; ++target_index) {
        for (unsigned current = 0; current < 2; ++current) {
            for (unsigned automatic = 0; automatic < 2; ++automatic) {
                duel_view_t view = {.mode = DUEL_MODE_DUO, .phase = DUEL_PHASE_TARGET,
                    .target_ms = 1000 + target_index * 500, .target_visible = true,
                    .round = 1, .current = current, .winner = -1};
                duel_ui_render(&view, 67, true, true, false);
                lv_obj_update_layout(lv_screen_active());
                lv_refr_now(display);
                uint16_t target_pixels[192 * 105];
                for (unsigned row = 0; row < 105; ++row) {
                    memcpy(target_pixels + row * 192, s_frame + (102 + row) * 240 + 24,
                           192 * sizeof(*target_pixels));
                }
                view.phase = DUEL_PHASE_HANDOVER;
                view.finished[1 - current] = true;
                view.automatic[1 - current] = automatic;
                duel_ui_render(&view, 67, true, true, false);
                lv_obj_update_layout(lv_screen_active());
                check_labels(lv_screen_active());
                lv_refr_now(display);
                char duration[16];
                snprintf(duration, sizeof(duration), "%u.%u s", (unsigned)(view.target_ms / 1000),
                         (unsigned)(view.target_ms % 1000 / 100));
                lv_obj_t *number = find_label(duration);
                if (!number || !find_label("目标时长") || find_label("VS")) {
                    fprintf(stderr, "Handover must show the large target card without VS: %s\n", duration);
                    exit(1);
                }
                assert(lv_obj_get_style_text_font(number, 0) == &lv_font_montserrat_40);
                lv_obj_t *panel = lv_obj_get_parent(number);
                assert(lv_obj_get_width(panel) == 192 && lv_obj_get_height(panel) == 105);
                assert(lv_obj_get_x(panel) == 24 && lv_obj_get_y(panel) == 71);
                assert(find_label("训练成绩暂不公开") && find_label("OK 开始计时"));
                assert(find_label(automatic ? "上一位已自动停止" : "接过终端，按 OK 开始"));
                lv_obj_t *objects[64];
                const unsigned count = collect_objects(lv_screen_active(), objects, 0);
                unsigned images = 0;
                for (unsigned index = 0; index < count; ++index) {
                    if (lv_obj_check_type(objects[index], &lv_image_class)) ++images;
                }
                assert(images == 1);
                for (unsigned row = 0; row < 105; ++row) {
                    assert(memcmp(target_pixels + row * 192, s_frame + (102 + row) * 240 + 24,
                                  192 * sizeof(*target_pixels)) == 0);
                }
            }
        }
    }
    lv_screen_load(fallback);
    duel_ui_destroy();
    puts("LVGL handover: 20 target-card pixel comparisons, 40 px numbers, no VS/agents PASS");
}

static void check_battery_only(lv_display_t *display)
{
    lv_obj_t *before_objects[64];
    const unsigned object_count = collect_objects(lv_screen_active(), before_objects, 0);
    lv_obj_t *battery = NULL;
    lv_anim_t *animation = NULL;
    int32_t animation_time = 0;
    for (unsigned index = 0; index < object_count; ++index) {
        lv_obj_t *object = before_objects[index];
        if (lv_obj_check_type(object, &lv_label_class) &&
            lv_obj_get_x(object) == 185 && lv_obj_get_y(object) == 7) battery = object;
        if (lv_anim_get(object, NULL)) {
            animation = lv_anim_get(object, NULL);
            animation_time = animation->act_time;
        }
    }
    assert(battery);
    const unsigned animations = lv_anim_count_running();
    uint16_t before_frame[240 * 320];
    memcpy(before_frame, s_frame, sizeof(before_frame));
    const int readings[] = {67, 67, 66, -1, 101, -9, 0, 100, 67};
    for (unsigned reading = 0; reading < sizeof(readings) / sizeof(*readings); ++reading) {
        const int value = readings[reading];
        duel_ui_set_battery(value);
        char expected[16];
        if (value >= 0 && value <= 100) snprintf(expected, sizeof(expected), "%d%%", value);
        else snprintf(expected, sizeof(expected), "--%%");
        assert(strcmp(lv_label_get_text(battery), expected) == 0);
        lv_refr_now(display);
        for (unsigned row = 0; row < 320; ++row) {
            for (unsigned column = 0; column < 240; ++column) {
                if (column >= 185 && column < 235 && row >= 7 && row < 28) continue;
                const unsigned pixel = row * 240 + column;
                assert(s_frame[pixel] == before_frame[pixel]);
            }
        }
        const unsigned flushes = s_flushes;
        duel_ui_set_battery(value);
        lv_refr_now(display);
        assert(s_flushes == flushes);
        lv_obj_t *after_objects[64];
        assert(collect_objects(lv_screen_active(), after_objects, 0) == object_count);
        assert(memcmp(before_objects, after_objects, object_count * sizeof(*before_objects)) == 0);
        assert(lv_anim_count_running() == animations);
        if (animation) {
            assert(lv_anim_get(animation->var, NULL) == animation);
            assert(animation->act_time == animation_time);
        }
    }
}

static void save_frame(const char *directory, unsigned frame)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/duel-%02u.ppm", directory, frame);
    FILE *output = fopen(path, "wb");
    assert(output);
    fprintf(output, "P6\n240 320\n255\n");
    for (unsigned pixel = 0; pixel < 240 * 320; ++pixel) {
        const uint16_t value = s_frame[pixel];
        const uint8_t rgb[3] = {(uint8_t)(((value >> 11) & 31) * 255 / 31),
                                (uint8_t)(((value >> 5) & 63) * 255 / 63),
                                (uint8_t)((value & 31) * 255 / 31)};
        assert(fwrite(rgb, 1, 3, output) == 3);
    }
    assert(fclose(output) == 0);
}

int main(int argc, char **argv)
{
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw, NULL, sizeof(s_draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_obj_t *fallback = lv_screen_active();
    check_target_handover_parity(display);
    size_t baseline_free = 0;
    size_t peak_used = 0;
    for (unsigned cycle = 0; cycle < 30; ++cycle) {
        assert(duel_ui_create());
        const unsigned phase_count = DUEL_PHASE_MATCH_WIN + 1;
        for (unsigned variant = 0; variant < phase_count * 3; ++variant) {
            const unsigned phase = variant % phase_count;
            const bool alternate = variant >= phase_count;
            const bool tied = variant >= phase_count * 2 && phase != DUEL_PHASE_MATCH_WIN;
            duel_view_t view = {
                .mode = alternate ? DUEL_MODE_AI : DUEL_MODE_DUO,
                .phase = (duel_phase_t)phase,
                .round = 5,
                .current = alternate ? 1 : 0,
                .score = {phase >= DUEL_PHASE_ROUND_END && !tied && !alternate ? 3 : 2,
                          phase >= DUEL_PHASE_ROUND_END && !tied && alternate ? 3 : 2},
                .target_ms = 2000,
                .target_visible = phase == DUEL_PHASE_TARGET || phase == DUEL_PHASE_HANDOVER || phase >= DUEL_PHASE_SEALED,
                .results_visible = phase >= DUEL_PHASE_RESULT,
                .elapsed_ms = {1982, 2145},
                .error_ms = {18, 145},
                .winner = tied ? -1 : alternate ? 1 : 0,
                .automatic = {alternate, alternate},
            };
            if (!view.target_visible) view.target_ms = 0;
            if (!view.results_visible) {
                if (phase != DUEL_PHASE_ROUND_END) view.winner = -1;
                memset(view.elapsed_ms, 0, sizeof(view.elapsed_ms));
                memset(view.error_ms, 0, sizeof(view.error_ms));
            }
            duel_ui_render(&view, alternate ? -1 : 87, alternate, variant < phase_count * 2, alternate);
            lv_obj_update_layout(lv_screen_active());
            check_labels(lv_screen_active());
            lv_refr_now(display);
            if (phase == DUEL_PHASE_TIMING || phase == DUEL_PHASE_SEALED) {
                assert(lv_anim_count_running() == 0);
                uint16_t frozen[240 * 320];
                memcpy(frozen, s_frame, sizeof(frozen));
                lv_tick_inc(700);
                lv_timer_handler();
                lv_refr_now(display);
                assert(memcmp(frozen, s_frame, sizeof(frozen)) == 0);
            }
            if (argc > 1 && cycle == 0) save_frame(argv[1], variant);
            check_battery_only(display);
            lv_tick_inc(180);
            lv_timer_handler();
            assert(lv_mem_test() == LV_RESULT_OK);
            lv_mem_monitor_t during_render;
            lv_mem_monitor(&during_render);
            if (during_render.max_used > peak_used) peak_used = during_render.max_used;
        }
        lv_screen_load(fallback);
        duel_ui_destroy();
        duel_ui_set_battery(23);
        lv_tick_inc(2000);
        lv_timer_handler();
        lv_mem_monitor_t monitor;
        lv_mem_monitor(&monitor);
        if (cycle == 0) baseline_free = monitor.free_size;
        else assert(monitor.free_size == baseline_free);
    }
    printf("LVGL UI: %u renders PASS; 30 teardown cycles PASS; timing/sealed static PASS; "
           "battery-only pixels, duplicate suppression and animation preservation PASS; "
           "24 KiB pool free: %zu bytes; peak used: %zu bytes\n",
           (unsigned)(DUEL_PHASE_MATCH_WIN + 1) * 90, baseline_free, peak_used);
    lv_display_delete(display);
    lv_deinit();
    return 0;
}
