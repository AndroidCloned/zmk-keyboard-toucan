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

/*
 * Battery % stabiliser for the gem chips.
 *
 * ZMK raises peripheral SoC=0 on every split disconnect, and also briefly
 * reports 0 before the first ADC sample on boot. Painting those zeros is what
 * makes the R chip drop from e.g. 73% → 0%. Rules:
 *   - 255 = unknown (draw as "--") until first non-zero sample
 *   - Peripheral (R): keep last good % across disconnect/beacon zeros; after
 *     30s with no non-zero SoC, expire to "--" (true disconnect / dead link)
 *   - Local (L): require several consecutive 0s before accepting empty
 *   - Ignore ±1 ADC/BAS chatter
 */
#define BATT_PCT_UNKNOWN 255

struct batt_sticky {
	uint8_t display;
	uint8_t zero_streak;
	bool have;
};

static uint8_t batt_sticky_apply(struct batt_sticky *s, uint8_t level, bool ignore_all_zeros)
{
	if (level == 0) {
		if (!s->have) {
			return BATT_PCT_UNKNOWN;
		}
		if (ignore_all_zeros) {
			/* Split disconnect + peripheral BAS boot / charge beacon. */
			return s->display;
		}
		if (s->zero_streak < 3) {
			s->zero_streak++;
			return s->display;
		}
		s->display = 0;
		return s->display;
	}

	s->zero_streak = 0;

	if (!s->have) {
		s->display = level;
		s->have = true;
		return s->display;
	}

	if (level + 1 < s->display || level > s->display + 1) {
		s->display = level;
	}

	return s->display;
}

static struct batt_sticky local_batt_sticky;
static struct batt_sticky periph_batt_sticky;

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
	uint8_t raw = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge();

	return (struct battery_status_state){
		.level = batt_sticky_apply(&local_batt_sticky, raw, false),
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
 * Right charge flag: toucan_peripheral_vbus.c encodes VBUS in BAS LSB
 * (odd = charging, even/100 = not). Sticky ±1 hides parity in the %.
 * Disconnect still publishes 0 → keep last good, expire to "--" after stale.
 */
#define PERIPH_STALE_MS 30000

static int64_t periph_last_good_at;

static void battery_peripheral_status_update_cb(struct battery_peripheral_status_state state);
static void periph_stale_expire_cb(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(periph_stale_expire_work, periph_stale_expire_cb);

static void periph_publish(uint8_t level, bool charging)
{
	battery_peripheral_status_update_cb((struct battery_peripheral_status_state){
		.level = level,
		.charging = charging,
	});
}

static void periph_stale_expire_cb(struct k_work *work)
{
	ARG_UNUSED(work);
	int64_t now = k_uptime_get();

	if (periph_last_good_at != 0 && (now - periph_last_good_at) < PERIPH_STALE_MS) {
		(void)k_work_schedule(&periph_stale_expire_work,
				      K_MSEC(PERIPH_STALE_MS - (now - periph_last_good_at)));
		return;
	}

	if (!periph_batt_sticky.have) {
		return;
	}

	periph_batt_sticky.have = false;
	periph_batt_sticky.display = 0;
	periph_publish(BATT_PCT_UNKNOWN, false);
}

static void set_battery_peripheral_status(struct zmk_widget_screen *widget,
					  struct battery_peripheral_status_state state)
{
	widget->state.battery_p = state.level;
	widget->state.charging_p = state.charging;
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
	uint8_t shown;
	int64_t now = k_uptime_get();
	bool charging = false;

	if (ev != NULL) {
		level = ev->state_of_charge;
	} else if (IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)) {
		(void)zmk_split_central_get_peripheral_battery_level(0, &level);
	}

	if (level == 0) {
		if (periph_batt_sticky.have) {
			(void)k_work_schedule(&periph_stale_expire_work, K_MSEC(PERIPH_STALE_MS));
		}
		shown = batt_sticky_apply(&periph_batt_sticky, level, true);
	} else {
		periph_last_good_at = now;
		(void)k_work_cancel_delayable(&periph_stale_expire_work);
		/* Odd BAS level ⇒ VBUS/charging (100 is always not-charging). */
		charging = (level != 100) && ((level & 1) != 0);
		shown = batt_sticky_apply(&periph_batt_sticky, level, false);
	}

	if (level == 0 && periph_batt_sticky.have && periph_last_good_at != 0 &&
	    (now - periph_last_good_at) >= PERIPH_STALE_MS) {
		periph_batt_sticky.have = false;
		periph_batt_sticky.display = 0;
		shown = BATT_PCT_UNKNOWN;
		charging = false;
	}

	return (struct battery_peripheral_status_state){
		.level = shown,
		.charging = charging,
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
	widget->state.battery = BATT_PCT_UNKNOWN;
	widget->state.battery_p = BATT_PCT_UNKNOWN;

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
