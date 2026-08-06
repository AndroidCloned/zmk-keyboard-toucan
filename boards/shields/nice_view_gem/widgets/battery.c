#include <zephyr/kernel.h>

#include "battery.h"
#include "util.h"

void draw_battery_status(lv_obj_t *canvas, const struct status_state *state)
{
	uint8_t level = state->battery > 100 ? 100 : state->battery;

	/* Shared Y with R chip for top-row symmetry. */
	canvas_draw_battery_chip(canvas, 8, 8, 'L', level, state->charging);
}
