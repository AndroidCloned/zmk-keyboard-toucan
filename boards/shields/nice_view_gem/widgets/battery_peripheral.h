#pragma once

#include <lvgl.h>
#include "util.h"

struct battery_peripheral_status_state {
	uint8_t level;
	bool charging;
};

void draw_battery_peripheral_status(lv_obj_t *canvas, const struct status_state *state);
