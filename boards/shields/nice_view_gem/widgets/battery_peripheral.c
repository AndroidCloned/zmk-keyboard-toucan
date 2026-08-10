#include <zephyr/kernel.h>

#include "battery_peripheral.h"
#include "util.h"

void draw_battery_peripheral_status(lv_obj_t *canvas, const struct status_state *state)
{
	uint8_t level = state->battery_p;

	if (level > 100 && level < 255) {
		level = 100;
	}

	/* Same Y/size as L chip (12×12 at y=8). */
	canvas_draw_battery_chip(canvas, 80, 8, 'R', level, state->charging_p);
}
