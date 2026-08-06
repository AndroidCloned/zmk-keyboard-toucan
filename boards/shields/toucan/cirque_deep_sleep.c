/*
 * Cirque Pinnacle power for Toucan right.
 *
 * SPI stays at 1 MHz — exit Sleep/Shutdown with FORCE_WAKEUP (GT-AN-090620 /
 * petejohanson/cirque-input-module#2). IDLE → EN_SLEEP; SLEEP/soft-off →
 * SHUTDOWN; boot SYS_INIT wakes ASIC before pinnacle_init (RAM flags die on
 * System OFF reboot).
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/activity.h>

LOG_MODULE_REGISTER(toucan_cirque_pm, CONFIG_ZMK_LOG_LEVEL);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(glidepoint), okay)

#define PINNACLE_REG_FIRMWARE_ID            0x00
#define PINNACLE_REG_STATUS1                0x02
#define PINNACLE_REG_SYS_CONFIG1            0x03
#define PINNACLE_REG_FEED_CONFIG1           0x04

#define PINNACLE_FIRMWARE_ID                0x07

#define PINNACLE_SYS_CONFIG1_RESET          BIT(0)
#define PINNACLE_SYS_CONFIG1_SHUTDOWN       BIT(1)
#define PINNACLE_SYS_CONFIG1_EN_SLEEP       BIT(2)
#define PINNACLE_SYS_CONFIG1_WAKEUP_TOGGLE  BIT(6)
#define PINNACLE_SYS_CONFIG1_FORCE_WAKEUP   BIT(7)

#define PINNACLE_FEED_CONFIG1_FEED_ENABLE   BIT(0)

#define PINNACLE_SPI_FB    0xFB
#define PINNACLE_READ_MSK  0xA0
#define PINNACLE_WRITE_MSK 0x80
#define PINNACLE_READ_REG(addr)  (PINNACLE_READ_MSK | (addr))
#define PINNACLE_WRITE_REG(addr) (PINNACLE_WRITE_MSK | (addr))

#define PINNACLE_RAP_GAP_US 50
#define PINNACLE_CLEAR_SETTLE_MS 1
#define PINNACLE_FORCE_SETTLE_MS 3

#define PINNACLE_SPI_OP                                                                                \
	(SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_MODE_CPHA | SPI_WORD_SET(8))

/* Literal prio: after SPI (~40–50), before INPUT (~90). */
#define CIRQUE_BOOT_PREWAKE_PRIO 60

static const struct spi_dt_spec cirque_spi =
	SPI_DT_SPEC_GET(DT_NODELABEL(glidepoint), PINNACLE_SPI_OP, 0);

static bool cirque_sleep_armed;

static void cirque_rap_gap(void)
{
	k_usleep(PINNACLE_RAP_GAP_US);
}

static int cirque_write_reg(uint8_t address, uint8_t value)
{
	uint8_t tx_data[] = {
		PINNACLE_WRITE_REG(address),
		value,
	};
	const struct spi_buf tx_buf[] = {{
		.buf = tx_data,
		.len = sizeof(tx_data),
	}};
	const struct spi_buf_set tx_set = {
		.buffers = tx_buf,
		.count = ARRAY_SIZE(tx_buf),
	};
	int rc = spi_write_dt(&cirque_spi, &tx_set);

	cirque_rap_gap();
	return rc;
}

static int cirque_read_reg(uint8_t address, uint8_t *value)
{
	uint8_t tx_data[] = {
		PINNACLE_READ_REG(address),
		PINNACLE_SPI_FB,
		PINNACLE_SPI_FB,
		PINNACLE_SPI_FB,
	};
	const struct spi_buf tx_buf[] = {{
		.buf = tx_data,
		.len = sizeof(tx_data),
	}};
	const struct spi_buf_set tx_set = {
		.buffers = tx_buf,
		.count = ARRAY_SIZE(tx_buf),
	};
	const struct spi_buf rx_buf[] = {
		{
			.buf = NULL,
			.len = 3,
		},
		{
			.buf = value,
			.len = 1,
		},
	};
	const struct spi_buf_set rx_set = {
		.buffers = rx_buf,
		.count = ARRAY_SIZE(rx_buf),
	};
	int rc = spi_transceive_dt(&cirque_spi, &tx_set, &rx_set);

	cirque_rap_gap();
	return rc;
}

static int cirque_clear_status1(void)
{
	return cirque_write_reg(PINNACLE_REG_STATUS1, 0x00);
}

static int cirque_feed_enable(void)
{
	uint8_t feed_cfg;
	int rc = cirque_read_reg(PINNACLE_REG_FEED_CONFIG1, &feed_cfg);

	if (rc) {
		return rc;
	}
	if ((feed_cfg & PINNACLE_FEED_CONFIG1_FEED_ENABLE) != 0) {
		return 0;
	}
	feed_cfg |= PINNACLE_FEED_CONFIG1_FEED_ENABLE;
	return cirque_write_reg(PINNACLE_REG_FEED_CONFIG1, feed_cfg);
}

static int cirque_feed_disable(void)
{
	uint8_t feed_cfg;
	int rc = cirque_read_reg(PINNACLE_REG_FEED_CONFIG1, &feed_cfg);

	if (rc) {
		return rc;
	}
	if ((feed_cfg & PINNACLE_FEED_CONFIG1_FEED_ENABLE) == 0) {
		return 0;
	}
	feed_cfg &= ~PINNACLE_FEED_CONFIG1_FEED_ENABLE;
	return cirque_write_reg(PINNACLE_REG_FEED_CONFIG1, feed_cfg);
}

/* restore_host: clear Status1 + feed when the input driver is already live. */
static int cirque_hw_force_awake(bool restore_host)
{
	uint8_t sys_cfg = 0xFF;
	int rc;

	if (!spi_is_ready_dt(&cirque_spi)) {
		LOG_ERR("Cirque SPI not ready for wake");
		return -ENODEV;
	}

	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, 0x00);
	if (rc) {
		LOG_ERR("Cirque SYS_CONFIG1 clear failed: %d", rc);
		return rc;
	}
	k_msleep(PINNACLE_CLEAR_SETTLE_MS);

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	if (rc) {
		LOG_WRN("Cirque SYS_CONFIG1 read after clear failed (%d) — force", rc);
		sys_cfg = 0xFF;
	}

	if (sys_cfg != 0x00) {
		LOG_WRN("Cirque SysConfig1 0x%02x — FORCE_WAKEUP", sys_cfg);
		bool forced = false;

		for (int tries = 8; tries > 0; tries--) {
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
			if ((sys_cfg & PINNACLE_SYS_CONFIG1_FORCE_WAKEUP) != 0) {
				forced = true;
				break;
			}
		}

		if (!forced) {
			LOG_ERR("Cirque FORCE_WAKEUP did not stick");
			return -ETIMEDOUT;
		}

		rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, 0x00);
		if (rc) {
			return rc;
		}
		k_msleep(PINNACLE_CLEAR_SETTLE_MS);
	}

	if (restore_host) {
		(void)cirque_clear_status1();
		rc = cirque_feed_enable();
		if (rc) {
			LOG_ERR("Cirque FEED_CONFIG1 enable failed: %d", rc);
			return rc;
		}
	}

	cirque_sleep_armed = false;
	LOG_INF("Cirque ASIC awake (restore_host=%d)", restore_host);
	return 0;
}

static int cirque_exit_sleep_soft(void)
{
	uint8_t sys_cfg = 0xFF;
	int rc;

	if (!spi_is_ready_dt(&cirque_spi)) {
		return -ENODEV;
	}

	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, 0x00);
	if (rc) {
		return rc;
	}

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	if (rc) {
		return rc;
	}

	if ((sys_cfg & (PINNACLE_SYS_CONFIG1_SHUTDOWN | PINNACLE_SYS_CONFIG1_EN_SLEEP)) != 0) {
		LOG_WRN("Soft sleep exit incomplete (0x%02x) — escalating", sys_cfg);
		k_msleep(PINNACLE_CLEAR_SETTLE_MS);
		return cirque_hw_force_awake(true);
	}

	cirque_sleep_armed = false;
	LOG_INF("Cirque Sleep Enable cleared (soft)");
	return 0;
}

static int cirque_ensure_awake(void)
{
	uint8_t sys_cfg = 0;
	uint8_t fw_id = 0;
	int rc;

	if (!spi_is_ready_dt(&cirque_spi)) {
		return -ENODEV;
	}

	/* IDLE→ACTIVE after we set EN_SLEEP — skip probe. */
	if (cirque_sleep_armed) {
		return cirque_exit_sleep_soft();
	}

	/* Post–System OFF or unknown: probe hardware. */
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

	cirque_sleep_armed = false;
	return 0;
}

static int cirque_enter_sleep(void)
{
	int rc;

	if (!spi_is_ready_dt(&cirque_spi)) {
		return -ENODEV;
	}

	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, PINNACLE_SYS_CONFIG1_EN_SLEEP);
	if (rc) {
		LOG_ERR("Cirque Sleep Enable write failed: %d", rc);
		return rc;
	}

	cirque_sleep_armed = true;
	LOG_INF("Cirque Sleep Enable set");
	return 0;
}

static int cirque_force_shutdown(void)
{
	uint8_t sys_cfg;
	int rc;

	if (!spi_is_ready_dt(&cirque_spi)) {
		return -ENODEV;
	}

	(void)cirque_feed_disable();

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	if (rc) {
		LOG_ERR("Cirque SYS_CONFIG1 read failed: %d", rc);
		return rc;
	}

	sys_cfg &= ~(PINNACLE_SYS_CONFIG1_EN_SLEEP | PINNACLE_SYS_CONFIG1_FORCE_WAKEUP |
		     PINNACLE_SYS_CONFIG1_WAKEUP_TOGGLE | PINNACLE_SYS_CONFIG1_RESET);
	sys_cfg |= PINNACLE_SYS_CONFIG1_SHUTDOWN;
	rc = cirque_write_reg(PINNACLE_REG_SYS_CONFIG1, sys_cfg);
	if (rc) {
		LOG_ERR("Cirque SHUTDOWN write failed: %d", rc);
		return rc;
	}

	k_msleep(5);
	(void)cirque_clear_status1();
	cirque_sleep_armed = false;
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
		(void)cirque_enter_sleep();
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

/* Before pinnacle_init: Cirque can still be SHUTDOWN after MCU System OFF. */
static int cirque_boot_prewake(void)
{
	uint8_t sys_cfg = 0;
	uint8_t fw_id = 0;
	int rc;

	if (!spi_is_ready_dt(&cirque_spi)) {
		return 0;
	}

	rc = cirque_read_reg(PINNACLE_REG_SYS_CONFIG1, &sys_cfg);
	(void)cirque_read_reg(PINNACLE_REG_FIRMWARE_ID, &fw_id);

	if (rc != 0 || (sys_cfg & PINNACLE_SYS_CONFIG1_SHUTDOWN) != 0 ||
	    fw_id != PINNACLE_FIRMWARE_ID) {
		LOG_WRN("Boot prewake FORCE (sys=0x%02x fw=0x%02x rc=%d)", sys_cfg, fw_id,
			rc);
		(void)cirque_hw_force_awake(false);
	} else {
		LOG_INF("Boot prewake ok (sys=0x%02x fw=0x%02x)", sys_cfg, fw_id);
	}

	return 0;
}

SYS_INIT(cirque_boot_prewake, POST_KERNEL, CIRQUE_BOOT_PREWAKE_PRIO);

#endif /* glidepoint okay */
