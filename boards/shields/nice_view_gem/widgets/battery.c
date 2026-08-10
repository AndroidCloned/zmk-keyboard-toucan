#include <zephyr/kernel.h>

#include "battery.h"
#include "util.h"

void draw_battery_status(lv_obj_t *canvas, const struct status_state *state)
{
	/* 255 = unknown (draw "--"); only clamp bogus 101–254. */
	uint8_t level = state->battery;

	if (level > 100 && level < 255) {
		level = 100;
	}

	/* Shared Y with R chip for top-row symmetry. */
	canvas_draw_battery_chip(canvas, 8, 8, 'L', level, state->charging);
}
