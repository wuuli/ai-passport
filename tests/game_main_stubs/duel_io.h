#pragma once
#include <stdbool.h>

bool duel_io_init(bool audio_ready, bool battery_ready);
void duel_io_activate(bool active);
int duel_io_battery(void);
