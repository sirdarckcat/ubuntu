// SPDX-License-Identifier: GPL-2.0
/*
 *  qed_fpga_fw.h - QED fpga fw download.
 *
 *  Copyright (C) 2024 Lantronix Inc.
 *
 */
#ifndef QED_FPGA__FW_H_
#define QED_FPGA__FW_H_

#include <asm/unaligned.h>

extern bool fpga_fw_load_done;

#define QED_SPI_FPGA_MAX_ECP5_ITER 30
#define QED_ECP5_FW_LOAD_ITER 6

static int ecp5_spi_cmd_a(struct spi_device *spi_dev, unsigned char cmd,
			  unsigned int *val)
{
	unsigned char rx[8] = {0};
	unsigned char tx[8] = {0};

	struct spi_transfer t[] = {
		{
			.tx_buf = &tx,
			.len = 8,
			.cs_change = 0, /* do not hold low */
			.bits_per_word = 8,
			.rx_buf = &rx,
		},
	};

	int ret;

	tx[0] = cmd;

	ret = spi_sync_transfer(spi_dev, t, ARRAY_SIZE(t));
	if (ret < 0) {
		pr_debug("Read: Failed to send in tx "
		         "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
			 tx[0], tx[1], tx[2], tx[3], tx[4], tx[5], tx[6], tx[7]);
		return ret;
	}
	pr_debug("ecp5: Read: rx: "
	         "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		 rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);

	*val = get_unaligned_be32(&rx[4]);

	return ret;
}

static int ecp5_spi_cmd_c(struct spi_device *spi_dev, unsigned char cmd)
{
	uint32_t tx = cmd;

	return spi_write(spi_dev, &tx, sizeof(tx));
}

static int ecp5_spi_write_fw_stream(struct spi_device *spi_dev,
				    const u8 * const data, unsigned int size)
{
	int rc;
	u8 *ptr = NULL;
	uint32_t burst = CMD_LSC_BITSTREAM_BURST;

	ptr = kzalloc(size + sizeof(burst), GFP_KERNEL);
	if (!ptr) {
		pr_err("NO MEM ENOMEM\n");
		return -ENOMEM;
	}

	memcpy(ptr, &burst, sizeof(burst));
	memcpy(ptr + sizeof(burst), data, size);

	rc = spi_write(spi_dev, (void*)ptr, size + sizeof(burst));
	kfree(ptr);
	return rc;
}

/* for cmd_clear and cmd_refresh */
static int __maybe_unused ecp5_spi_cmd_d(struct spi_device *spi_dev, unsigned char cmd)
{
	uint32_t tx = cmd;

	if (spi_write(spi_dev, &tx, sizeof(tx)) != 0) {
		pr_err("Write: Failed to write: %02x%02x%02x%02x\n",
		       ((uint8_t *)&tx)[0], ((uint8_t *)&tx)[1],
		 ((uint8_t *)&tx)[2], ((uint8_t *)&tx)[3]);
		return -1;
	}

	msleep(20);
	return 0;
}

static int spi_init_fw (struct platform_device *pdev)
{
	int retries = QED_SPI_FPGA_MAX_ECP5_ITER;
	struct device *dev = &pdev->dev;
	struct fpga_data *pd = platform_get_drvdata(pdev);
	uint32_t sleep_step = 1;

	gpiod_set_value(pd->programn, 0);
	gpiod_set_value(pd->power, 0);
	msleep(2);
	gpiod_set_value(pd->power, 1);
	/* delay for power set */
	msleep(25);

	while (--retries) {
		msleep(sleep_step);
		if (!gpiod_get_value(pd->initn))
			break;
	}

	if (!retries) {
		dev_err(dev, "Retries \"INITN pin to low level\" limit reached in %u ms\n",
			(QED_SPI_FPGA_MAX_ECP5_ITER - 1 ) * sleep_step);
		return -1;
	}

	gpiod_set_value(pd->programn, 1);

	retries = QED_SPI_FPGA_MAX_ECP5_ITER;
	while (--retries) {
		msleep(sleep_step);
		if (gpiod_get_value(pd->initn)) {
			break;
		}
	}

	if (!retries) {
		dev_err(dev, "Retries \"INITN pin to high\" limit reached in %u ms\n",
			(QED_SPI_FPGA_MAX_ECP5_ITER - 1 ) * sleep_step);
		return -1;
	}

	msleep(5);
	return 0;
}

static int spi_deinit_fw (struct platform_device *pdev)
{
	struct fpga_data *pd = platform_get_drvdata(pdev);
	gpiod_set_value(pd->programn, 0);
	gpiod_set_value(pd->power, 0);
	return 0;
}

static int decode_status (struct device *dev, uint32_t status)
{
	uint32_t mask = 0x01;
	uint32_t mask_3 = 0x07;
	uint8_t shift = 0;
	uint8_t res;
	int rc = 0;

	dev_dbg(dev, "Status:%08x\n",
		 status);
	res = (status >> shift++) & mask;

	if (res)
		dev_dbg(dev, "Status: Transparent Mode: Yes\n");
	else
		dev_dbg(dev, "Status: Transparent Mode: No\n");

	res = ((status >> shift++) & mask_3);

	if (!res)
		dev_dbg(dev, "Status: Config Target Selection: SRAM\n");
	else if (res == 0x1)
		dev_dbg(dev, "Status: Config Target Selection: eFuse\n");
	else
		dev_dbg(dev, "Status: Config Target Selection: UNKNOWN\n");

	shift += 2;
	res = (status >> shift++) & mask;

	if (res)
		dev_dbg(dev, "Status: JTAC Active: Yes\n");
	else
		dev_dbg(dev, "Status: JTAC Active: No\n");

	res = (status >> shift++) & mask;

	if (res)
		dev_dbg(dev, "Status: PWD Protections: Yes\n");
	else
		dev_dbg(dev, "Status: PWD Protections: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Internal use 1\n");
	else
		dev_dbg(dev, "Status: Internal use 0\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Decrypt Enable: Yes\n");
	else
		dev_dbg(dev, "Status: Decrypt Enable: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: DONE: Yes/Set\n");
	else
		dev_dbg(dev, "Status: DONE: No/Not set\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: ISC Enabled: Yes\n");
	else
		dev_dbg(dev, "Status: ISC Enabled: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Write Enabled: Yes\n");
	else
		dev_dbg(dev, "Status: Write Enabled: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Read Enabled: Yes\n");
	else
		dev_dbg(dev, "Status: Read Enabled: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Busy Flag: Yes\n");
	else
		dev_dbg(dev, "Status: Busy Flag: No\n");

	res = (status >> shift++) & mask;
	if (res) {
		dev_info(dev, "Status: Fail Flag: Yes\n");
		rc = -1;
	}
	else
		dev_dbg(dev, "Status: Fail Flag: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: FEA OTP: Yes\n");
	else
		dev_dbg(dev, "Status: FEA OTP: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Decrypt Only: Yes\n");
	else
		dev_dbg(dev, "Status: Decrypt Only: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: PWD Enable: Yes\n");
	else
		dev_dbg(dev, "Status: PWD Enable: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Internal use 1\n");
	else
		dev_dbg(dev, "Status: Internal use 0\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Internal use 1\n");
	else
		dev_dbg(dev, "Status: Internal use 0\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Internal use 1\n");
	else
		dev_dbg(dev, "Status: Internal use 0\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Encrypt Preamble: Yes\n");
	else
		dev_dbg(dev, "Status: Encrypt Preamble: No\n");

	res = (status >> shift++) & mask;
	if (res)
		dev_dbg(dev, "Status: Std Preamble: Yes\n");
	else
		dev_dbg(dev, "Status: Std Preamble: No\n");

	res = (status >> shift++) & mask;
	if (res) {
		dev_err(dev, "Status: SPIm Fail 1: Yes\n");
		rc = -1;
	}
	else
		dev_dbg(dev, "Status: SPIm Fail 1: No\n");

	res = (status >> shift++) & mask_3;
	switch (res) {
		case 0:
			dev_dbg(dev, "Status: BSE status Code: "
				 "NONE\n");
			break;
		case 0x1:
			rc = -1;
			dev_info(dev, "Status: BSE status Code: "
				 "ID status\n");
			break;
		case 0x2:
			rc = -1;
			dev_info(dev, "Status: BSE status Code: "
				 "CMD status: illegal command\n");
			break;
		case 0x3:
			rc = -1;
			dev_info(dev, "Status: BSE status Code: "
				 "CRC status\n");
			break;
		case 0x4:
			rc = -1;
			dev_info(dev, "Status: BSE status Code: "
				 "PRMB status - preabmle status\n");
			break;
		case 0x5:
			rc = -1;
			dev_info(dev, "Status: BSE status Code: "
				 "ABRT status - configuration aborted by the user\n");
			break;
		case 0x6:
			rc = -1;
			dev_info(dev,
				 "Status: BSE status Code: "
				 "OVFL status - data overflow status\n");
			break;
		case 0x7:
			rc = -1;
			dev_info(dev,
				 "Status: BSE status Code: "
				 "SDM status - bitstream pass the size of the SRAM array\n");
			break;
		default:
			rc = -1;
			dev_info(dev,
				 "Status: BSE status Code: "
				 "UNKNOWN\n");
			break;
	}

	shift += 2;
	res = (status >> shift++) & mask;

	if (res) {
		rc = -1;
		dev_info(dev, "Status: Execution status: Yes\n");
	}
	else
		dev_dbg(dev, "Status: Execution status: No\n");

	res = (status >> shift++) & mask;

	if (res) {
		rc = -1;
		dev_info(dev, "Status: ID status: "
			 "ID mismatch with Verify_ID command: Yes\n");
	}

	res = (status >> shift++) & mask;

	if (res) {
		rc = -1;
		dev_info(dev, "Status: Invalid Command: Yes\n");
	}

	res = (status >> shift++) & mask;

	if (res) {
		rc = -1;
		dev_info(dev, "Status: SED status: Yes\n");
	}

	res = (status >> shift++) & mask;

	if (res)
		dev_dbg(dev, "Status: Bypass Mode: Yes\n");
	else
		dev_dbg(dev, "Status: Bypass Mode: No\n");

	res = (status >> shift++) & mask;

	if (res)
		dev_dbg(dev, "Status: Flow Through Mode: Yes\n");
	else
		dev_dbg(dev, "Status: Flow Through Mode: No\n");

	return rc;
}

static int status_done (uint32_t status)
{
	uint32_t mask = 0x100;
	return !(status & mask);
}

static int __maybe_unused firmware_load (struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct firmware *fw = NULL;
	const char *fw_name = NULL;
	struct	spi_master *fw_master;
	unsigned char cmd;
	unsigned int val;
	u8 retries = QED_ECP5_FW_LOAD_ITER;
	int rc = 0;

	struct fpga_data *state = platform_get_drvdata(pdev);

	fw_master = spi_busnum_to_master(state->fw_info.bus_num);

	if (!fw_master) {
		spi_deinit_fw(pdev);
		rc = -1;
		dev_err(dev, "SPI fw_master not found.\n");
		return rc;
	}

	state->spi_fw = spi_new_device(fw_master, &state->fw_info);

	if (!state->spi_fw) {
		spi_deinit_fw(pdev);
		rc = -1;
		dev_err(dev, "Failed to create slave.\n");
		return rc;
	}

	rc = spi_setup(state->spi_fw);

	if (rc) {
		spi_deinit_fw(pdev);
		dev_err(dev, "Failed to setup slave.\n");
		return rc;
	}

	while (--retries) {

		rc = spi_init_fw(pdev);

		if (rc) {
			dev_err(dev, "spi init fw failed\n");
			break;
		}

		cmd = CMD_READ_ID;
		rc = ecp5_spi_cmd_a(state->spi_fw, cmd, &val);

		if (rc) {
			dev_err(dev, "Failed to send command %02x\n", cmd);
			break;
		}

		dev_info(dev, "ecp5 id: 0x%08x", val);

		/* Select firmware based on FPGA ID */
		switch (val) {
		case LFE5U_45_ID:
		case LFE5UM_45_ID:
			fw_name = ECP5_FM;
			break;
		case LFE5U_25_ID:
		case LFE5UM_25_ID:
			fw_name = ECP5_FM_25;
			break;
		default:
			dev_err(dev,
				"Read FPGA_ID: 0x%08x does not match known IDs "
				"(LFE5U_45_ID: 0x%08x, LFE5UM_45_ID: 0x%08x)",
				val, LFE5U_45_ID, LFE5UM_45_ID);
			rc = -1;
			continue;
		}

		if (fw == NULL) {
			rc = request_firmware(&fw, fw_name, dev);
			if (rc) {
				dev_err(dev, "Failed to request firmware %s\n", fw_name);
				break;
			}

			dev_info(dev, "Loaded firmware: %s\n", fw_name);
		}

		cmd = CMD_ISC_ENABLE;
		rc = ecp5_spi_cmd_c(state->spi_fw, cmd);

		if (rc) {
			dev_err(dev, "Failed to send ISC_ENABLE\n");
			break;
		}

		rc = ecp5_spi_write_fw_stream(state->spi_fw, fw->data, fw->size);

		if (rc) {
			dev_warn(dev, "Failed to write %zu byte fw stream\n", fw->size);
			msleep(5);
			continue;
		}

		cmd = CMD_READ_STATUS;
		rc = ecp5_spi_cmd_a(state->spi_fw, cmd, &val);

		if (rc) {
			dev_err(dev, "Failed to send READ_STATUS\n");
			break;
		}

		rc = decode_status(dev, val);

		if (rc) {
			dev_warn(dev, "Status error: %08x\n", val);
			continue;
		}

		rc = status_done(val);
		if (rc) {
			dev_warn(dev, "ecp5 status not done\n");
			msleep(5);
			continue;
		}

		dev_dbg(dev, "ecp5 status: %08x done: %d\n", val, rc);
		dev_dbg(dev, "ecp5 done gpio: %d\n", gpiod_get_value(state->done));

		cmd = CMD_ISC_DISABLE;
		rc = ecp5_spi_cmd_c(state->spi_fw, cmd);

		if (rc) {
			dev_warn(dev, "Failed to finalize FPGA fw on retry %d\n",
				 QED_ECP5_FW_LOAD_ITER - retries);
			continue;
		}

		dev_dbg(dev, "Loaded FPGA firmware on retry %d\n",
				QED_ECP5_FW_LOAD_ITER - retries);
		break;
	}

	dev_dbg(dev, "Load FPGA total retries %d\n",
		 QED_ECP5_FW_LOAD_ITER - (retries ? retries : retries - 1));

	if (state->spi_fw) {
		spi_unregister_device(state->spi_fw);
	}

	if (fw) {
		release_firmware(fw);
	}

	return rc;
}
#endif /* QED_FPGA__FW_H_ */
