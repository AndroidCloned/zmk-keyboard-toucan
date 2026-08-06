#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#include "layer.h"
#include "../assets/custom_fonts.h"
#include <zmk/keymap.h>

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state)
{
	lv_draw_label_dsc_t label_dsc;
	char fallback_layer_name[16];
	const char *layer_name =
		zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state->layer_index));

	init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_24, LV_TEXT_ALIGN_CENTER);

	if (layer_name == NULL || layer_name[0] == '\0') {
		snprintk(fallback_layer_name, sizeof(fallback_layer_name), "L#%" PRIu8,
			 state->layer_index);
		layer_name = fallback_layer_name;
	}

	canvas_draw_text(canvas, 0, 70, SCREEN_WIDTH, &label_dsc, layer_name);
}
