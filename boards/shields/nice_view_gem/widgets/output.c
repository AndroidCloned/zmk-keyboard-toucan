#include <zephyr/kernel.h>
#include "output.h"
#include "../assets/custom_fonts.h"

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void draw_usb_connected(lv_obj_t *canvas)
{
	lv_draw_label_dsc_t label_dsc;
	init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
	canvas_draw_text(canvas, 12, 140, SCREEN_WIDTH - 8, &label_dsc, "USB");
}
#endif

static void draw_ble_label(lv_obj_t *canvas, const char *text)
{
	lv_draw_label_dsc_t label_dsc;
	init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
	canvas_draw_text(canvas, 12, 140, SCREEN_WIDTH - 8, &label_dsc, text);
}

void draw_output_status(lv_obj_t *canvas, const struct status_state *state)
{
	switch (state->selected_endpoint.transport) {
	case ZMK_TRANSPORT_USB:
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
		draw_usb_connected(canvas);
#else
		draw_ble_label(canvas, "USB");
#endif
		break;
	case ZMK_TRANSPORT_BLE:
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
		if (!state->active_profile_bonded) {
			draw_ble_label(canvas, "PAIR");
		} else if (state->active_profile_connected) {
			draw_ble_label(canvas, "BLE");
		} else {
			draw_ble_label(canvas, "WAIT");
		}
#else
		draw_ble_label(canvas, state->connected ? "BLE" : "WAIT");
#endif
		break;
	default:
		draw_ble_label(canvas, "NULL");
		break;
	}
}
