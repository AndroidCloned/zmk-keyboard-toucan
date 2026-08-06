#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/split/central.h>

#include "battery.h"
#include "battery_peripheral.h"
#include "layer.h"
#include "output.h"
#include "profile.h"
#include "screen.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void draw_top(lv_obj_t *widget, const struct status_state *state)
{
	lv_obj_t *canvas = lv_obj_get_child(widget, 0);

	fill_background(canvas);
	canvas_frame_begin(canvas);
	draw_output_status(canvas, state);
	draw_layer_status(canvas, state);
	draw_profile_status(canvas, state);
	draw_battery_status(canvas, state);
	draw_battery_peripheral_status(canvas, state);
	canvas_frame_end();
	lv_obj_invalidate(canvas);
}

/* Coalesce rapid battery/layer/BLE updates into one LVGL paint on the display WQ. */
static void redraw_all_work_cb(struct k_work *work)
{
	ARG_UNUSED(work);
	struct zmk_widget_screen *widget;

	SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
		draw_top(widget->obj, &widget->state);
	}
}

static K_WORK_DEFINE(redraw_all_work, redraw_all_work_cb);

static void schedule_redraw(void)
{
	if (zmk_display_is_initialized()) {
		k_work_submit_to_queue(zmk_display_work_q(), &redraw_all_work);
	}
}

static void set_battery_status(struct zmk_widget_screen *widget, struct battery_status_state state)
{
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
	widget->state.charging = state.usb_present;
#endif
	widget->state.battery = state.level;
	schedule_redraw();
}

static void battery_status_update_cb(struct battery_status_state state)
{
	struct zmk_widget_screen *widget;

	SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
		set_battery_status(widget, state);
	}
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh)
{
	const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

	return (struct battery_status_state){
		.level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
		.usb_present = zmk_usb_is_powered(),
#endif
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
			    battery_status_update_cb, battery_status_get_state);
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

/*
 * Right half has no reliable charge flag over split BAS (SoC only). Inferring
 * “charging” from a rising % false-triggered on ADC / load-recovery bounce.
 * R “+” stays off; L still uses USB presence. Revisit if we add a real VBUS
 * signal that does not pulse BAS.
 */
static uint8_t periph_batt_display;
static bool periph_batt_have_display;

static void set_battery_peripheral_status(struct zmk_widget_screen *widget,
					  struct battery_peripheral_status_state state)
{
	widget->state.battery_p = state.level;
	widget->state.charging_p = false;
	schedule_redraw();
}

static void battery_peripheral_status_update_cb(struct battery_peripheral_status_state state)
{
	struct zmk_widget_screen *widget;

	SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
		set_battery_peripheral_status(widget, state);
	}
}

static struct battery_peripheral_status_state battery_peripheral_status_get_state(const zmk_event_t *eh)
{
	const struct zmk_peripheral_battery_state_changed *ev =
		as_zmk_peripheral_battery_state_changed(eh);
	uint8_t level = 0;

	if (ev != NULL) {
		level = ev->state_of_charge;
	} else {
		(void)zmk_split_central_get_peripheral_battery_level(0, &level);
	}

	/* Sticky %: ignore ±1 flaps. */
	if (!periph_batt_have_display || level + 1 < periph_batt_display ||
	    level > periph_batt_display + 1) {
		periph_batt_display = level;
	}
	periph_batt_have_display = true;

	return (struct battery_peripheral_status_state){
		.level = periph_batt_display,
		.charging = false,
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_peripheral_status,
			    struct battery_peripheral_status_state,
			    battery_peripheral_status_update_cb,
			    battery_peripheral_status_get_state);
ZMK_SUBSCRIPTION(widget_battery_peripheral_status, zmk_peripheral_battery_state_changed);

static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state)
{
	widget->state.layer_index = state.index;
	schedule_redraw();
}

static void layer_status_update_cb(struct layer_status_state state)
{
	struct zmk_widget_screen *widget;

	SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
		set_layer_status(widget, state);
	}
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh)
{
	ARG_UNUSED(eh);
	return (struct layer_status_state){.index = zmk_keymap_highest_layer_active()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
			    layer_status_get_state);
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

static void set_output_status(struct zmk_widget_screen *widget,
			      const struct output_status_state *state)
{
	widget->state.selected_endpoint = state->selected_endpoint;
	widget->state.active_profile_index = state->active_profile_index;
	widget->state.active_profile_connected = state->active_profile_connected;
	widget->state.active_profile_bonded = state->active_profile_bonded;
	schedule_redraw();
}

static void output_status_update_cb(struct output_status_state state)
{
	struct zmk_widget_screen *widget;

	SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
		set_output_status(widget, &state);
	}
}

static struct output_status_state output_status_get_state(const zmk_event_t *eh)
{
	ARG_UNUSED(eh);
	return (struct output_status_state){
		.selected_endpoint = zmk_endpoint_get_selected(),
		.active_profile_index = zmk_ble_active_profile_index(),
		.active_profile_connected = zmk_ble_active_profile_is_connected(),
		.active_profile_bonded = !zmk_ble_active_profile_is_open(),
	};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
			    output_status_update_cb, output_status_get_state);
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent)
{
	widget->obj = lv_obj_create(parent);
	lv_obj_set_size(widget->obj, SCREEN_WIDTH, SCREEN_HEIGHT);

	lv_obj_t *top = lv_canvas_create(widget->obj);
	lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
	lv_canvas_set_buffer(top, widget->cbuf, SCREEN_WIDTH, SCREEN_HEIGHT, CANVAS_COLOR_FORMAT);

	sys_slist_append(&widgets, &widget->node);
	widget_battery_status_init();
	widget_battery_peripheral_status_init();
	widget_layer_status_init();
	widget_output_status_init();

	/* Initial paint on the display work queue (same path as live updates). */
	schedule_redraw();

	return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget)
{
	return widget->obj;
}
