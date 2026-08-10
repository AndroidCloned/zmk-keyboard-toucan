/*
 * Cirque power: SLEEP/soft-off → SHUTDOWN; boot FORCE_WAKEUP before
 * pinnacle_init. After init: restore CAL_CONFIG1 defaults + feed (do not
 * wipe compensation bits or re-FORCE_WAKEUP — both caused drunk/unusable
 * relative tracking after System OFF). IDLE leaves ASIC sensing.
 */

#include "pinnacle.h"

#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

LOG_MODULE_REGISTER(toucan_cirque_pm, CONFIG_ZMK_LOG_LEVEL);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(glidepoint), okay)

static bool sleep_armed;

void cirque_sleep_armed_clear(void)
{
	sleep_armed = false;
}

int cirque_restore_cal_config(void)
{
	if (!cirque_spi_ready()) {
		return -ENODEV;
	}

	return cirque_write_reg(PINNACLE_REG_CAL_CONFIG1, PINNACLE_CAL_CONFIG1_DEFVAL);
}

int cirque_hw_force_awake(bool restore_host)
{
	uint8_t sys_cfg = 0xFF;
	uint8_t fw_id = 0;
	int rc;
	bool forced = false;

	if (!cirque_spi_ready()) {
		LOG_ERR("Cirque SPI not ready for wake");
		return -ENODEV;
	}

	/* Clear Sleep/Shutdown/force bits first. */
	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, 0x00);
	if (rc) {
		LOG_ERR("Cirque SYS_CONFIG1 clear failed: %d", rc);
		return rc;
	}
	k_msleep(PINNACLE_CLEAR_SETTLE_MS);

	/*
	 * Always assert FORCE_WAKEUP at 1 MHz — do not trust a post-clear
	 * SYS_CONFIG1==0 read. That early-out left the ASIC in SHUTDOWN after
	 * System OFF so pinnacle_init saw a bad FW ID and the pad stayed dead.
	 */
	for (int tries = 12; tries > 0; tries--) {
		rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1,
				      PINNACLE_SYS_CONFIG1_FORCE_WAKEUP |
					      PINNACLE_SYS_CONFIG1_WAKEUP_TOGGLE);
		if (rc) {
			LOG_ERR("Cirque force-wake write failed: %d", rc);
			return rc;
		}
		k_msleep(PINNACLE_FORCE_SETTLE_MS);

		rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
		if (rc) {
			continue;
		}
		if ((sys_cfg & PINNACLE_SYS_CONFIG1_FORCE_WAKEUP) != 0 ||
		    (sys_cfg & PINNACLE_SYS_CONFIG1_SHUTDOWN) == 0) {
			forced = true;
			break;
		}
	}

	if (!forced) {
		LOG_ERR("Cirque FORCE_WAKEUP did not stick (sys=0x%02x)", sys_cfg);
		return -ETIMEDOUT;
	}

	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, 0x00);
	if (rc) {
		return rc;
	}
	k_msleep(PINNACLE_CLEAR_SETTLE_MS);

	rc = cirque_read_reg(PINNACLE_REG_FIRMWARE_ID, &fw_id);
	if (rc || fw_id != PINNACLE_FIRMWARE_ID) {
		LOG_WRN("Cirque FW ID after wake 0x%02x (rc=%d)", fw_id, rc);
	}

	if (restore_host) {
		(void)cirque_recalibrate();
		(void)cirque_clear_status1();
		rc = cirque_feed_set(true);
		if (rc) {
			LOG_ERR("Cirque FEED_CONFIG1 enable failed: %d", rc);
			return rc;
		}
	}

	sleep_armed = false;
	LOG_INF("Cirque ASIC awake (restore_host=%d fw=0x%02x)", restore_host, fw_id);
	return 0;
}

int cirque_recalibrate(void)
{
	uint8_t status = 0;
	int rc;
	int tries;

	if (!cirque_spi_ready()) {
		return -ENODEV;
	}

	(void)cirque_feed_set(false);

	/*
	 * Must preserve Cirque’s compensation enables. Writing CALIBRATE alone
	 * clears BACKGROUND/NERD/TAP/PALM bits and leaves relative tracking
	 * unusable until the next full RESET.
	 */
	rc = cirque_write_reg(PINNACLE_REG_CAL_CONFIG1,
			      PINNACLE_CAL_CONFIG1_DEFVAL | PINNACLE_CAL_CONFIG1_CALIBRATE);
	if (rc) {
		LOG_ERR("Cirque CALIBRATE write failed: %d", rc);
		(void)cirque_restore_cal_config();
		(void)cirque_feed_set(true);
		return rc;
	}

	for (tries = PINNACLE_CAL_WAIT_TRIES; tries > 0; tries--) {
		k_msleep(PINNACLE_CAL_POLL_MS);
		rc = cirque_read_reg(PINNACLE_REG_STATUS1, &status);
		if (rc == 0 && (status & PINNACLE_STATUS1_SW_CC) != 0) {
			break;
		}
	}

	(void)cirque_restore_cal_config();
	(void)cirque_clear_status1();
	k_msleep(PINNACLE_CLEAR_SETTLE_MS);
	(void)cirque_clear_status1();

	if (tries == 0) {
		LOG_WRN("Cirque recalibrate timeout (status=0x%02x)", status);
		(void)cirque_feed_set(true);
		return -ETIMEDOUT;
	}

	LOG_INF("Cirque recalibrated (comp enables restored)");
	return 0;
}

static int cirque_exit_sleep_soft(void)
{
	uint8_t sys_cfg = 0xFF;
	int rc;

	if (!cirque_spi_ready()) {
		return -ENODEV;
	}

	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, 0x00);
	if (rc) {
		return rc;
	}
	k_msleep(PINNACLE_CLEAR_SETTLE_MS);

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	if (rc) {
		return cirque_hw_force_awake(true);
	}

	if ((sys_cfg & (PINNACLE_SYS_CONFIG1_SHUTDOWN | PINNACLE_SYS_CONFIG1_EN_SLEEP)) != 0) {
		LOG_WRN("Soft sleep exit incomplete (0x%02x) — escalating", sys_cfg);
		return cirque_hw_force_awake(true);
	}

	sleep_armed = false;
	LOG_INF("Cirque Sleep Enable cleared (soft)");
	return 0;
}

int cirque_ensure_awake(void)
{
	uint8_t sys_cfg = 0;
	uint8_t fw_id = 0;
	int rc;

	if (!cirque_spi_ready()) {
		return -ENODEV;
	}

	if (sleep_armed) {
		return cirque_exit_sleep_soft();
	}

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	if (rc) {
		LOG_WRN("Cirque probe read failed (%d) — force wake", rc);
		return cirque_hw_force_awake(true);
	}

	if ((sys_cfg & PINNACLE_SYS_CONFIG1_SHUTDOWN) != 0 ||
	    (sys_cfg & (PINNACLE_SYS_CONFIG1_FORCE_WAKEUP | PINNACLE_SYS_CONFIG1_WAKEUP_TOGGLE)) !=
		    0) {
		return cirque_hw_force_awake(true);
	}

	if ((sys_cfg & PINNACLE_SYS_CONFIG1_EN_SLEEP) != 0) {
		return cirque_exit_sleep_soft();
	}

	rc = cirque_read_reg(PINNACLE_REG_FIRMWARE_ID, &fw_id);
	if (rc || fw_id != PINNACLE_FIRMWARE_ID) {
		LOG_WRN("Cirque FW ID 0x%02x (rc=%d) — force wake", fw_id, rc);
		return cirque_hw_force_awake(true);
	}

	sleep_armed = false;
	return 0;
}

int cirque_enter_sleep(void)
{
	int rc;

	if (!cirque_spi_ready()) {
		return -ENODEV;
	}

	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, PINNACLE_SYS_CONFIG1_EN_SLEEP);
	if (rc) {
		LOG_ERR("Cirque Sleep Enable write failed: %d", rc);
		return rc;
	}

	sleep_armed = true;
	LOG_INF("Cirque Sleep Enable set");
	return 0;
}

int cirque_force_shutdown(void)
{
	uint8_t sys_cfg;
	int rc;

	if (!cirque_spi_ready()) {
		return -ENODEV;
	}

	(void)cirque_feed_set(false);

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	if (rc) {
		LOG_ERR("Cirque SYS_CONFIG1 read failed: %d", rc);
		/* Still try a blind SHUTDOWN write. */
		sys_cfg = 0;
	}

	sys_cfg &= ~(PINNACLE_SYS_CONFIG1_EN_SLEEP | PINNACLE_SYS_CONFIG1_FORCE_WAKEUP |
		     PINNACLE_SYS_CONFIG1_WAKEUP_TOGGLE | PINNACLE_SYS_CONFIG1_RESET);
	sys_cfg |= PINNACLE_SYS_CONFIG1_SHUTDOWN;
	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, sys_cfg);
	if (rc) {
		LOG_ERR("Cirque SHUTDOWN write failed: %d", rc);
		return rc;
	}

	k_msleep(PINNACLE_SHUTDOWN_SETTLE_MS);
	sleep_armed = false;
	LOG_INF("Cirque SHUTDOWN SYS_CONFIG1=0x%02x", sys_cfg);
	return 0;
}

static int cirque_activity_listener(const zmk_event_t *eh)
{
	const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);

	if (ev == NULL) {
		return ZMK_EV_EVENT_BUBBLE;
	}

	switch (ev->state) {
	case ZMK_ACTIVITY_SLEEP:
		(void)cirque_force_shutdown();
		break;
	case ZMK_ACTIVITY_ACTIVE:
		(void)cirque_ensure_awake();
		break;
	case ZMK_ACTIVITY_IDLE:
		/*
		 * PROD: leave Cirque in Active/Idle sensing (~1.7 mA). EN_SLEEP on
		 * ZMK idle caused unreliable touch wake / “dead pad” reports; the
		 * big win is SHUTDOWN at System OFF (~0.23 µA), not 30s Sleep.
		 */
		break;
	}

	return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(toucan_cirque_pm, cirque_activity_listener);
ZMK_SUBSCRIPTION(toucan_cirque_pm, zmk_activity_state_changed);

#if IS_ENABLED(CONFIG_ZMK_PM_SOFT_OFF)
int __real_zmk_pm_soft_off(void);

int __wrap_zmk_pm_soft_off(void)
{
	(void)cirque_force_shutdown();
	return __real_zmk_pm_soft_off();
}
#endif

static int cirque_boot_prewake(void)
{
	if (!cirque_spi_ready()) {
		LOG_ERR("Cirque boot prewake: SPI not ready");
		return 0;
	}

	/* Always FORCE_WAKEUP — Cirque may still be in SHUTDOWN after System OFF. */
	LOG_INF("Cirque boot prewake FORCE_WAKEUP");
	(void)cirque_hw_force_awake(false);
	/* Give ASIC time before pinnacle_init RESET/calibration. */
	k_msleep(20);

	return 0;
}

SYS_INIT(cirque_boot_prewake, POST_KERNEL, CIRQUE_BOOT_PREWAKE_PRIO);

/*
 * After pinnacle_init: force-wake if still shut down; otherwise recalibrate
 * with the DT sensitivity already applied (ADC attenuation) + feed.
 */
static int cirque_boot_followup(void)
{
	uint8_t sys_cfg = 0;
	int rc;

	if (!cirque_spi_ready()) {
		return 0;
	}

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	if (rc || (sys_cfg & (PINNACLE_SYS_CONFIG1_SHUTDOWN | PINNACLE_SYS_CONFIG1_EN_SLEEP |
			      PINNACLE_SYS_CONFIG1_FORCE_WAKEUP |
			      PINNACLE_SYS_CONFIG1_WAKEUP_TOGGLE)) != 0) {
		LOG_INF("Cirque boot follow-up FORCE_WAKEUP+cal (sys=0x%02x rc=%d)", sys_cfg,
			rc);
		(void)cirque_hw_force_awake(true);
		return 0;
	}

	LOG_INF("Cirque boot follow-up recalibrate+feed");
	(void)cirque_recalibrate();
	(void)cirque_clear_status1();
	(void)cirque_feed_set(true);
	return 0;
}

SYS_INIT(cirque_boot_followup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* glidepoint */
