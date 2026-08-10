/*
 * Drop pointing over split while the peripheral link is down, and for a short
 * settle after connect (System OFF wake → reconnect dumps noisy relative
 * deltas). Open path after settle: one atomic_get + uptime compare.
 */

#include "pinnacle.h"

#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <zmk/event_manager.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/split/peripheral.h>
#include <zmk/split/transport/types.h>

LOG_MODULE_REGISTER(toucan_cirque_gate, CONFIG_ZMK_LOG_LEVEL);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(glidepoint), okay) && IS_ENABLED(CONFIG_ZMK_SPLIT) && \
	!IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static atomic_t split_tx_ok;
static int64_t pointing_ok_at_ms;

static void cirque_split_gate_set(bool connected)
{
	if (connected) {
		atomic_set(&split_tx_ok, 1);
		pointing_ok_at_ms = k_uptime_get() + CIRQUE_SPLIT_SETTLE_MS;

		if (cirque_spi_ready()) {
			(void)cirque_ensure_awake();
			(void)cirque_restore_cal_config();
			(void)cirque_clear_status1();
		}

		LOG_INF("Cirque split TX open (settle %d ms)", CIRQUE_SPLIT_SETTLE_MS);
	} else {
		atomic_set(&split_tx_ok, 0);
		pointing_ok_at_ms = 0;
		LOG_INF("Cirque split TX gated");
	}
}

static int cirque_split_status_listener(const zmk_event_t *eh)
{
	const struct zmk_split_peripheral_status_changed *ev =
		as_zmk_split_peripheral_status_changed(eh);

	if (ev == NULL) {
		return ZMK_EV_EVENT_BUBBLE;
	}

	cirque_split_gate_set(ev->connected);
	return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(toucan_cirque_split_gate, cirque_split_status_listener);
ZMK_SUBSCRIPTION(toucan_cirque_split_gate, zmk_split_peripheral_status_changed);

int __real_zmk_split_peripheral_report_event(
	const struct zmk_split_transport_peripheral_event *event);

int __wrap_zmk_split_peripheral_report_event(
	const struct zmk_split_transport_peripheral_event *event)
{
	if (event != NULL &&
	    event->type == ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_INPUT_EVENT) {
		if (atomic_get(&split_tx_ok) == 0) {
			return 0;
		}
		if (k_uptime_get() < pointing_ok_at_ms) {
			return 0;
		}
	}

	return __real_zmk_split_peripheral_report_event(event);
}

static int cirque_split_gate_sync(void)
{
	cirque_split_gate_set(zmk_split_bt_peripheral_is_connected());
	return 0;
}

SYS_INIT(cirque_split_gate_sync, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif
