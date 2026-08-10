/*
 * Right-half VBUS → R charging flag on the left gem.
 *
 * ZMK split BAS only carries SoC 0–100. Encode VBUS in the LSB:
 *   VBUS present  → odd level  ((soc & ~1) | 1), 100 → 99
 *   VBUS absent   → even level (soc & ~1), keep 100; never emit 0 for soc 1→2
 *
 * Left decodes charging = (level != 0 && level != 100 && (level & 1)).
 * Sticky ±1 on the gem hides the parity flip in the displayed %.
 *
 * Wrap bt_bas_set_battery_level so ZMK’s periodic BAS updates keep the bit.
 */

#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <hal/nrf_power.h>

#include <zmk/battery.h>

LOG_MODULE_REGISTER(toucan_r_vbus, CONFIG_ZMK_LOG_LEVEL);

#define VBUS_DEBOUNCE_POLLS 2
#define VBUS_POLL_SEC       1

static uint8_t vbus_hits;
static bool vbus_latched;
static struct k_work_delayable poll_work;

int __real_bt_bas_set_battery_level(uint8_t level);

static bool vbus_raw(void)
{
	return nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
}

static uint8_t encode_level(uint8_t soc, bool charging)
{
	if (soc == 0) {
		return 0;
	}
	if (soc >= 100) {
		return charging ? 99 : 100;
	}
	if (charging) {
		return (uint8_t)((soc & ~1) | 1);
	}
	/* Never publish 0 while we still have a live cell reading. */
	if (soc <= 1) {
		return 2;
	}
	return (uint8_t)(soc & ~1);
}

int __wrap_bt_bas_set_battery_level(uint8_t level)
{
	return __real_bt_bas_set_battery_level(encode_level(level, vbus_latched));
}

static void republish_soc(void)
{
	uint8_t soc = zmk_battery_state_of_charge();

	if (soc == 0) {
		return;
	}

	(void)bt_bas_set_battery_level(soc);
}

static void poll_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (vbus_raw()) {
		if (vbus_hits < VBUS_DEBOUNCE_POLLS) {
			vbus_hits++;
		}
	} else {
		vbus_hits = 0;
	}

	bool present = vbus_hits >= VBUS_DEBOUNCE_POLLS;

	if (present != vbus_latched) {
		vbus_latched = present;
		LOG_INF("VBUS %s — BAS LSB charge encode", present ? "on" : "off");
		republish_soc();
	}

	(void)k_work_schedule(&poll_work, K_SECONDS(VBUS_POLL_SEC));
}

static int toucan_peripheral_vbus_init(void)
{
	k_work_init_delayable(&poll_work, poll_handler);
	(void)k_work_schedule(&poll_work, K_MSEC(500));
	return 0;
}

SYS_INIT(toucan_peripheral_vbus_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
