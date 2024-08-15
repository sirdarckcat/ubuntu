// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) STMicroelectronics 2018 - All Rights Reserved
 * Author: Ludovic.barre@st.com for STMicroelectronics.
 */
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/reset.h>
#include <linux/scatterlist.h>
#include "mmci.h"

#define SDMMC_LLI_BUF_LEN	PAGE_SIZE
#define SDMMC_IDMA_BURST	BIT(MMCI_STM32_IDMABNDT_SHIFT)

struct sdmmc_lli_desc {
	u32 idmalar;
	u32 idmabase;
	u32 idmasize;
};

struct sdmmc_idma {
	dma_addr_t sg_dma;
	void *sg_cpu;
	dma_addr_t bounce_dma_addr;
	void *bounce_buf;
	bool use_bounce_buffer;
};

int sdmmc_idma_validate_data(struct mmci_host *host,
			     struct mmc_data *data)
{
	struct sdmmc_idma *idma = host->dma_priv;
	struct device *dev = mmc_dev(host->mmc);
	struct scatterlist *sg;
	int i;

	/*
	 * idma has constraints on idmabase & idmasize for each element
	 * excepted the last element which has no constraint on idmasize
	 */
	idma->use_bounce_buffer = false;
	for_each_sg(data->sg, sg, data->sg_len - 1, i) {
		if (!IS_ALIGNED(sg->offset, sizeof(u32)) ||
		    !IS_ALIGNED(sg->length, SDMMC_IDMA_BURST)) {
			dev_dbg(mmc_dev(host->mmc),
				"unaligned scatterlist: ofst:%x length:%d\n",
				data->sg->offset, data->sg->length);
			goto use_bounce_buffer;
		}
	}

	if (!IS_ALIGNED(sg->offset, sizeof(u32))) {
		dev_dbg(mmc_dev(host->mmc),
			"unaligned last scatterlist: ofst:%x length:%d\n",
			data->sg->offset, data->sg->length);
		goto use_bounce_buffer;
	}

	return 0;

use_bounce_buffer:
	if (!idma->bounce_buf) {
		idma->bounce_buf = dmam_alloc_coherent(dev,
						       host->mmc->max_req_size,
						       &idma->bounce_dma_addr,
						       GFP_KERNEL);
		if (!idma->bounce_buf) {
			dev_err(dev, "Unable to map allocate DMA bounce buffer.\n");
			return -ENOMEM;
		}
	}

	idma->use_bounce_buffer = true;

	return 0;
}

static int _sdmmc_idma_prep_data(struct mmci_host *host,
				 struct mmc_data *data)
{
	struct sdmmc_idma *idma = host->dma_priv;

	if (idma->use_bounce_buffer) {
		if (data->flags & MMC_DATA_WRITE) {
			unsigned int xfer_bytes = data->blksz * data->blocks;

			sg_copy_to_buffer(data->sg, data->sg_len,
					  idma->bounce_buf, xfer_bytes);
			dma_wmb();
		}
	} else {
		int n_elem;

		n_elem = dma_map_sg(mmc_dev(host->mmc),
				    data->sg,
				    data->sg_len,
				    mmc_get_dma_dir(data));

		if (!n_elem) {
			dev_err(mmc_dev(host->mmc), "dma_map_sg failed\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int sdmmc_idma_prep_data(struct mmci_host *host,
				struct mmc_data *data, bool next)
{
	/* Check if job is already prepared. */
	if (!next && data->host_cookie == host->next_cookie)
		return 0;

	return _sdmmc_idma_prep_data(host, data);
}

static void sdmmc_idma_unprep_data(struct mmci_host *host,
				   struct mmc_data *data, int err)
{
	struct sdmmc_idma *idma = host->dma_priv;

	if (idma->use_bounce_buffer) {
		if (data->flags & MMC_DATA_READ) {
			unsigned int xfer_bytes = data->blksz * data->blocks;

			sg_copy_from_buffer(data->sg, data->sg_len,
					    idma->bounce_buf, xfer_bytes);
		}
	} else {
		dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
			     mmc_get_dma_dir(data));
	}
}

static int sdmmc_idma_setup(struct mmci_host *host)
{
	struct sdmmc_idma *idma;

	idma = devm_kzalloc(mmc_dev(host->mmc), sizeof(*idma), GFP_KERNEL);
	if (!idma)
		return -ENOMEM;

	host->dma_priv = idma;

	if (host->variant->dma_lli) {
		idma->sg_cpu = dmam_alloc_coherent(mmc_dev(host->mmc),
						   SDMMC_LLI_BUF_LEN,
						   &idma->sg_dma, GFP_KERNEL);
		if (!idma->sg_cpu) {
			dev_err(mmc_dev(host->mmc),
				"Failed to alloc IDMA descriptor\n");
			return -ENOMEM;
		}
		host->mmc->max_segs = SDMMC_LLI_BUF_LEN /
			sizeof(struct sdmmc_lli_desc);
		host->mmc->max_seg_size = host->variant->stm32_idmabsize_mask;

		host->mmc->max_req_size = SZ_1M;
	} else {
		host->mmc->max_segs = 1;
		host->mmc->max_seg_size = host->mmc->max_req_size;
	}

	return 0;
}

static int sdmmc_idma_start(struct mmci_host *host, unsigned int *datactrl)

{
	struct sdmmc_idma *idma = host->dma_priv;
	struct sdmmc_lli_desc *desc = (struct sdmmc_lli_desc *)idma->sg_cpu;
	struct mmc_data *data = host->data;
	struct scatterlist *sg;
	int i;

	host->dma_in_progress = true;

	if (!host->variant->dma_lli || data->sg_len == 1 ||
	    idma->use_bounce_buffer) {
		u32 dma_addr;

		if (idma->use_bounce_buffer)
			dma_addr = idma->bounce_dma_addr;
		else
			dma_addr = sg_dma_address(data->sg);

		writel_relaxed(dma_addr,
			       host->base + MMCI_STM32_IDMABASE0R);
		writel_relaxed(MMCI_STM32_IDMAEN,
			       host->base + MMCI_STM32_IDMACTRLR);
		return 0;
	}

	for_each_sg(data->sg, sg, data->sg_len, i) {
		desc[i].idmalar = (i + 1) * sizeof(struct sdmmc_lli_desc);
		desc[i].idmalar |= MMCI_STM32_ULA | MMCI_STM32_ULS
			| MMCI_STM32_ABR;
		desc[i].idmabase = sg_dma_address(sg);
		desc[i].idmasize = sg_dma_len(sg);
	}

	/* notice the end of link list */
	desc[data->sg_len - 1].idmalar &= ~MMCI_STM32_ULA;

	dma_wmb();
	writel_relaxed(idma->sg_dma, host->base + MMCI_STM32_IDMABAR);
	writel_relaxed(desc[0].idmalar, host->base + MMCI_STM32_IDMALAR);
	writel_relaxed(desc[0].idmabase, host->base + MMCI_STM32_IDMABASE0R);
	writel_relaxed(desc[0].idmasize, host->base + MMCI_STM32_IDMABSIZER);
	writel_relaxed(MMCI_STM32_IDMAEN | MMCI_STM32_IDMALLIEN,
		       host->base + MMCI_STM32_IDMACTRLR);

	return 0;
}

static void sdmmc_idma_error(struct mmci_host *host)
{
	struct mmc_data *data = host->data;
	struct sdmmc_idma *idma = host->dma_priv;

	if (!dma_inprogress(host))
		return;

	writel_relaxed(0, host->base + MMCI_STM32_IDMACTRLR);
	host->dma_in_progress = false;
	data->host_cookie = 0;

	if (!idma->use_bounce_buffer)
		dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
			     mmc_get_dma_dir(data));
}

static void sdmmc_idma_finalize(struct mmci_host *host, struct mmc_data *data)
{
	if (!dma_inprogress(host))
		return;

	writel_relaxed(0, host->base + MMCI_STM32_IDMACTRLR);
	host->dma_in_progress = false;

	if (!data->host_cookie)
		sdmmc_idma_unprep_data(host, data, 0);
}

static void mmci_sdmmc_set_clkreg(struct mmci_host *host, unsigned int desired)
{
	unsigned int clk = 0, ddr = 0;

	if (host->mmc->ios.timing == MMC_TIMING_MMC_DDR52 ||
	    host->mmc->ios.timing == MMC_TIMING_UHS_DDR50)
		ddr = MCI_STM32_CLK_DDR;

	/*
	 * cclk = mclk / (2 * clkdiv)
	 * clkdiv 0 => bypass
	 * in ddr mode bypass is not possible
	 */
	if (desired) {
		if (desired >= host->mclk && !ddr) {
			host->cclk = host->mclk;
		} else {
			clk = DIV_ROUND_UP(host->mclk, 2 * desired);
			if (clk > MCI_STM32_CLK_CLKDIV_MSK)
				clk = MCI_STM32_CLK_CLKDIV_MSK;
			host->cclk = host->mclk / (2 * clk);
		}
	} else {
		/*
		 * while power-on phase the clock can't be define to 0,
		 * Only power-off and power-cyc deactivate the clock.
		 * if desired clock is 0, set max divider
		 */
		clk = MCI_STM32_CLK_CLKDIV_MSK;
		host->cclk = host->mclk / (2 * clk);
	}

	/* Set actual clock for debug */
	if (host->mmc->ios.power_mode == MMC_POWER_ON)
		host->mmc->actual_clock = host->cclk;
	else
		host->mmc->actual_clock = 0;

	if (host->mmc->ios.bus_width == MMC_BUS_WIDTH_4)
		clk |= MCI_STM32_CLK_WIDEBUS_4;
	if (host->mmc->ios.bus_width == MMC_BUS_WIDTH_8)
		clk |= MCI_STM32_CLK_WIDEBUS_8;

	clk |= MCI_STM32_CLK_HWFCEN;
	clk |= host->clk_reg_add;
	clk |= ddr;

	/*
	 * SDMMC_FBCK is selected when an external Delay Block is needed
	 * with SDR104.
	 */
	if (host->mmc->ios.timing >= MMC_TIMING_UHS_SDR50) {
		clk |= MCI_STM32_CLK_BUSSPEED;
		if (host->mmc->ios.timing == MMC_TIMING_UHS_SDR104) {
			clk &= ~MCI_STM32_CLK_SEL_MSK;
			clk |= MCI_STM32_CLK_SELFBCK;
		}
	}

	mmci_write_clkreg(host, clk);
}

static void mmci_sdmmc_set_pwrreg(struct mmci_host *host, unsigned int pwr)
{
	struct mmc_ios ios = host->mmc->ios;

	pwr = host->pwr_reg_add;

	if (ios.power_mode == MMC_POWER_OFF) {
		/* Only a reset could power-off sdmmc */
		reset_control_assert(host->rst);
		udelay(2);
		reset_control_deassert(host->rst);

		/*
		 * Set the SDMMC in Power-cycle state.
		 * This will make that the SDMMC_D[7:0], SDMMC_CMD and SDMMC_CK
		 * are driven low, to prevent the Card from being supplied
		 * through the signal lines.
		 */
		mmci_write_pwrreg(host, MCI_STM32_PWR_CYC | pwr);
	} else if (ios.power_mode == MMC_POWER_ON) {
		/*
		 * After power-off (reset): the irq mask defined in probe
		 * functionis lost
		 * ault irq mask (probe) must be activated
		 */
		writel(MCI_IRQENABLE | host->variant->start_err,
		       host->base + MMCIMASK0);

		/*
		 * After a power-cycle state, we must set the SDMMC in
		 * Power-off. The SDMMC_D[7:0], SDMMC_CMD and SDMMC_CK are
		 * driven high. Then we can set the SDMMC to Power-on state
		 */
		mmci_write_pwrreg(host, MCI_PWR_OFF | pwr);
		mdelay(1);
		mmci_write_pwrreg(host, MCI_PWR_ON | pwr);
	}
}

static u32 sdmmc_get_dctrl_cfg(struct mmci_host *host)
{
	u32 datactrl;

	datactrl = mmci_dctrl_blksz(host);

	if (host->mmc->card && mmc_card_sdio(host->mmc->card) &&
	    host->data->blocks == 1)
		datactrl |= MCI_DPSM_STM32_MODE_SDIO;
	else if (host->data->stop && !host->mrq->sbc)
		datactrl |= MCI_DPSM_STM32_MODE_BLOCK_STOP;
	else
		datactrl |= MCI_DPSM_STM32_MODE_BLOCK;

	return datactrl;
}

static struct mmci_host_ops sdmmc_variant_ops = {
	.validate_data = sdmmc_idma_validate_data,
	.prep_data = sdmmc_idma_prep_data,
	.unprep_data = sdmmc_idma_unprep_data,
	.get_datactrl_cfg = sdmmc_get_dctrl_cfg,
	.dma_setup = sdmmc_idma_setup,
	.dma_start = sdmmc_idma_start,
	.dma_finalize = sdmmc_idma_finalize,
	.dma_error = sdmmc_idma_error,
	.set_clkreg = mmci_sdmmc_set_clkreg,
	.set_pwrreg = mmci_sdmmc_set_pwrreg,
};

void sdmmc_variant_init(struct mmci_host *host)
{
	host->ops = &sdmmc_variant_ops;
}
