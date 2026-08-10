#include "pinnacle.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#if DT_NODE_HAS_STATUS(DT_NODELABEL(glidepoint), okay)

static const struct spi_dt_spec cirque_spi =
	SPI_DT_SPEC_GET(DT_NODELABEL(glidepoint), PINNACLE_SPI_OP, 0);

bool cirque_spi_ready(void)
{
	return spi_is_ready_dt(&cirque_spi);
}

static void cirque_rap_gap(void)
{
	k_usleep(PINNACLE_RAP_GAP_US);
}

int cirque_write_reg(uint8_t address, uint8_t value)
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

int cirque_read_reg(uint8_t address, uint8_t *value)
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
		{.buf = NULL, .len = 3},
		{.buf = value, .len = 1},
	};
	const struct spi_buf_set rx_set = {
		.buffers = rx_buf,
		.count = ARRAY_SIZE(rx_buf),
	};
	int rc = spi_transceive_dt(&cirque_spi, &tx_set, &rx_set);

	cirque_rap_gap();
	return rc;
}

int cirque_clear_status1(void)
{
	return cirque_write_reg(PINNACLE_REG_STATUS1, 0x00);
}

int cirque_feed_set(bool enable)
{
	uint8_t feed_cfg;
	int rc = cirque_read_reg(PINNACLE_REG_FEED_CONFIG1, &feed_cfg);

	if (rc) {
		return rc;
	}

	if (enable) {
		if ((feed_cfg & PINNACLE_FEED_CONFIG1_FEED_ENABLE) != 0) {
			return 0;
		}
		feed_cfg |= PINNACLE_FEED_CONFIG1_FEED_ENABLE;
	} else {
		if ((feed_cfg & PINNACLE_FEED_CONFIG1_FEED_ENABLE) == 0) {
			return 0;
		}
		feed_cfg &= ~PINNACLE_FEED_CONFIG1_FEED_ENABLE;
	}

	return cirque_write_reg(PINNACLE_REG_FEED_CONFIG1, feed_cfg);
}

#else /* !glidepoint */

bool cirque_spi_ready(void)
{
	return false;
}

int cirque_write_reg(uint8_t address, uint8_t value)
{
	ARG_UNUSED(address);
	ARG_UNUSED(value);
	return -ENODEV;
}

int cirque_read_reg(uint8_t address, uint8_t *value)
{
	ARG_UNUSED(address);
	ARG_UNUSED(value);
	return -ENODEV;
}

int cirque_clear_status1(void)
{
	return -ENODEV;
}

int cirque_feed_set(bool enable)
{
	ARG_UNUSED(enable);
	return -ENODEV;
}

#endif /* glidepoint */
