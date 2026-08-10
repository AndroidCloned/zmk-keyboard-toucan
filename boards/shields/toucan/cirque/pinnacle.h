/*
 * Cirque Pinnacle register map + RAP timing (Toucan right).
 * SPI stays at 1 MHz — Sleep/Shutdown exit needs FORCE_WAKEUP (GT-AN-090620).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

#define PINNACLE_REG_FIRMWARE_ID  0x00
#define PINNACLE_REG_STATUS1      0x02
#define PINNACLE_REG_SYS_CONFIG1  0x03
#define PINNACLE_REG_FEED_CONFIG1 0x04
#define PINNACLE_REG_CAL_CONFIG1  0x07

#define PINNACLE_FIRMWARE_ID 0x07

#define PINNACLE_STATUS1_SW_DR BIT(2)
#define PINNACLE_STATUS1_SW_CC BIT(3)

#define PINNACLE_SYS_CONFIG1_RESET         BIT(0)
#define PINNACLE_SYS_CONFIG1_SHUTDOWN      BIT(1)
#define PINNACLE_SYS_CONFIG1_EN_SLEEP      BIT(2)
#define PINNACLE_SYS_CONFIG1_WAKEUP_TOGGLE BIT(6)
#define PINNACLE_SYS_CONFIG1_FORCE_WAKEUP  BIT(7)

#define PINNACLE_FEED_CONFIG1_FEED_ENABLE BIT(0)

/* HostReg CALCONFIG1 — Cirque default enables + CALIBRATE trigger. */
#define PINNACLE_CAL_CONFIG1_CALIBRATE              BIT(0)
#define PINNACLE_CAL_CONFIG1_BACKGROUND_COMP_ENABLE BIT(1)
#define PINNACLE_CAL_CONFIG1_NERD_COMP_ENABLE       BIT(2)
#define PINNACLE_CAL_CONFIG1_TRACK_ERROR_COMP       BIT(3)
#define PINNACLE_CAL_CONFIG1_TAP_COMP_ENABLE        BIT(4)
#define PINNACLE_CAL_CONFIG1_PALM_ERROR_COMP        BIT(5)
#define PINNACLE_CAL_CONFIG1_DEFVAL                                                                \
	(PINNACLE_CAL_CONFIG1_BACKGROUND_COMP_ENABLE | PINNACLE_CAL_CONFIG1_NERD_COMP_ENABLE |     \
	 PINNACLE_CAL_CONFIG1_TRACK_ERROR_COMP | PINNACLE_CAL_CONFIG1_TAP_COMP_ENABLE |            \
	 PINNACLE_CAL_CONFIG1_PALM_ERROR_COMP)

#define PINNACLE_SPI_FB    0xFB
#define PINNACLE_READ_MSK  0xA0
#define PINNACLE_WRITE_MSK 0x80
#define PINNACLE_READ_REG(addr)  (PINNACLE_READ_MSK | (addr))
#define PINNACLE_WRITE_REG(addr) (PINNACLE_WRITE_MSK | (addr))

#define PINNACLE_RAP_GAP_US         50
#define PINNACLE_CLEAR_SETTLE_MS    5
#define PINNACLE_FORCE_SETTLE_MS    10
#define PINNACLE_SHUTDOWN_SETTLE_MS 5
#define PINNACLE_CAL_POLL_MS        5
#define PINNACLE_CAL_WAIT_TRIES     40 /* ~200 ms, matches Cirque/QMK guidance */
/* Drop relative packets briefly after split comes up (post System OFF settle). */
#define CIRQUE_SPLIT_SETTLE_MS      300

#define PINNACLE_SPI_OP                                                                            \
	(SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_MODE_CPHA | SPI_WORD_SET(8))

/* After SPI (~40–50), before INPUT (~90). Literal — expression prios can fail. */
#define CIRQUE_BOOT_PREWAKE_PRIO 60

bool cirque_spi_ready(void);
int cirque_write_reg(uint8_t address, uint8_t value);
int cirque_read_reg(uint8_t address, uint8_t *value);
int cirque_clear_status1(void);
int cirque_feed_set(bool enable);
int cirque_restore_cal_config(void);

/* Power policy (activity / soft-off / boot). */
int cirque_hw_force_awake(bool restore_host);
int cirque_ensure_awake(void);
int cirque_recalibrate(void);
int cirque_enter_sleep(void);
int cirque_force_shutdown(void);
void cirque_sleep_armed_clear(void);
