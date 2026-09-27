#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create and show the game selection screen.
 * Safely loads the new screen before deleting any previous launcher screen.
 *
 * @param selected Initial selection (0: Exit 8 / Corridor, 1: Time Challenge / Duel).
 * @param battery  Current battery SoC (0-100) or -1 if unavailable.
 * @return true if screen was created successfully.
 */
bool game_launcher_create(unsigned selected, int battery);

/**
 * Update selection and battery reading.
 * Values are cached internally; no LVGL objects are invalidated unless a value changes.
 * Safe to call from a 10ms tick loop.
 *
 * @param selected Current selection (0: Corridor, 1: Duel).
 * @param battery  Battery percentage or -1.
 */
void game_launcher_update(unsigned selected, int battery);

/**
 * Destroy launcher screen and release all allocated widgets.
 * Resets all internal widget pointers before deleting the LVGL object.
 */
void game_launcher_destroy(void);

#ifdef __cplusplus
}
#endif
