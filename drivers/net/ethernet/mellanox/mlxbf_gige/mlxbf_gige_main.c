// SPDX-License-Identifier: GPL-2.0-only OR Linux-OpenIB

/* Gigabit Ethernet driver for Mellanox BlueField SoC
 *
 * Copyright (c) 2020, Mellanox Technologies
 */

/* Standard method to enable kernel debug (e.g. dev_dbg) */
#define DEBUG

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>

/* The MLXBF_GIGE_INTERNAL setting is defined in the
 * "mlxbf_gige.h" header file, so this header file must
 * be included before any processing of that setting.
 */
#include "mlxbf_gige.h"
#include "mlxbf_gige_regs.h"

#ifdef MLXBF_GIGE_INTERNAL
#include <linux/version.h>
/*
 * Upstreaming guidelines:
 * =======================
 * 1) Do not upstream any code that is encapsulated by
 *    the "MLXBF_GIGE_INTERNAL" tag; that tag is for code
 *    that is for internal use only.
 * 2) Remove all code that defines or checks for the
 *    definition of "FAST_MODELS". The code encapsulated
 *    by "#ifdef FAST_MODELS" should always be enabled
 *    in the upstream source since it is PHY related.
 * 3) Remove all code that defines or checks for the
 *    definition of "MLXBF_GIGE_LOOPBACK". The code encapsulated
 *    by "#ifndef MLXBF_GIGE_LOOPBACK" should always be enabled,
 *    i.e. upstream code should run in non-loopback mode.
 * 4) Remove any code that checks for current Linux version
 *    via "LINUX_VERSION_CODE". The upstream code should
 *    always be tailored to a specific Linux kernel.
 * 5) Remove "#define DEBUG" at top of this file.
 */

/* Define to create mmio read/write sysfs entries */
#define MLXBF_GIGE_MMIO_SYSFS

/* Define this to perform read/write tests to MMIO regs */
#define MLXBF_GIGE_MMIO_TESTS

/* Define this to perform read/write tests to LLU MMIO regs
 * NOTE: there is no LLU on FastModels, so don't try it.
 */
/* #define LLU_MMIO_TESTS */

/* Define this to perform read/write tests to PLU MMIO regs
 * NOTE: there is no PLU on FastModels, so don't try it.
 */
/* #define PLU_MMIO_TESTS */

/* Define this to put IP networking stack into loopback mode,
 * where IP stack will not transmit packets out the GigE interface.
 * Instead use the GigE sysfs entry (e.g. 'echo <n> > start_tx')
 * to send packets. It is assumed that interface is being put into
 * loopback mode by one of these methods:
 *   a) Fast Models loopback
 *   b) PLU loopback mode
 *   c) PHY loopback mode
 */
/* #define MLXBF_GIGE_LOOPBACK */
#endif /* MLXBF_GIGE_INTERNAL */

#define FAST_MODELS

#define DRV_NAME    "mlxbf_gige"
#define DRV_VERSION "1.0"

#ifdef MLXBF_GIGE_INTERNAL
#define MLXBF_GIGE_MSG_FORMAT \
	"  %02x %02x %02x %02x %02x %02x %02x %02x" \
	" %02x %02x %02x %02x %02x %02x %02x %02x\n"

#define MLXBF_GIGE_MSG_ARGS(p) \
	*p, *(p + 1), *(p + 2), *(p + 3), \
	*(p + 4), *(p + 5), *(p + 6), *(p + 7), \
	*(p + 8), *(p + 9), *(p + 10), *(p + 11), \
	*(p + 12), *(p + 13), *(p + 14), *(p + 15)

static void mlxbf_gige_plu_selftests(struct platform_device *pdev,
				     struct mlxbf_gige *priv)
{
#ifdef PLU_MMIO_TESTS
	u32 rd_data, exp_data;

	dev_dbg(&pdev->dev, "Running PLU MMIO tests\n");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->plu_base + 0x8);
	exp_data = 0x1ff;
	dev_dbg(&pdev->dev, "PLU 0x8 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->plu_base + 0x140);
	exp_data = 0xe8001870;
	dev_dbg(&pdev->dev, "PLU 0x140 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->plu_base + 0x610);
	exp_data = 0x31001;
	dev_dbg(&pdev->dev, "PLU 0x610 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->plu_base + 0x618);
	exp_data = 0xb0000;
	dev_dbg(&pdev->dev, "PLU 0x618 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->plu_base + 0x890);
	exp_data = 0x9;
	dev_dbg(&pdev->dev, "PLU 0x890 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->plu_base + 0x894);
	exp_data = 0x1;
	dev_dbg(&pdev->dev, "PLU 0x894 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");
#endif
}

static void mlxbf_gige_llu_selftests(struct platform_device *pdev,
				     struct mlxbf_gige *priv)
{
#ifdef LLU_MMIO_TESTS
	u32 rd_data, exp_data;

	dev_dbg(&pdev->dev, "Running LLU MMIO tests\n");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->llu_base + 0x2200);
	exp_data = 0x91008808;
	dev_dbg(&pdev->dev, "LLU 0x2200 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->llu_base + 0x2204);
	exp_data = 0x810088a8;
	dev_dbg(&pdev->dev, "LLU 0x2204 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->llu_base + 0x2208);
	exp_data = 0x22e90000;
	dev_dbg(&pdev->dev, "LLU 0x2208 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->llu_base + 0x220c);
	exp_data = 0x893f0000;
	dev_dbg(&pdev->dev, "LLU 0x220c equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->llu_base + 0x2260);
	exp_data = 0x8060806;
	dev_dbg(&pdev->dev, "LLU 0x2260 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readl(priv->llu_base + 0x2264);
	exp_data = 0x891422e7;
	dev_dbg(&pdev->dev, "LLU 0x2264 equals %x - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");
#endif
}

static void mlxbf_gige_selftests(struct platform_device *pdev,
				 struct mlxbf_gige *priv)
{
#ifdef MLXBF_GIGE_MMIO_TESTS
	u64 rd_data, wr_data, exp_data;

	dev_dbg(&pdev->dev, "Running MLXBF_GIGE MMIO tests\n");

	/* Read data should match reset value in register header file */
	rd_data = readq(priv->base + MLXBF_GIGE_CONFIG);
	exp_data = (MLXBF_GIGE_CONFIG_MAX_PKT_SZ_RESET_VAL
		    << MLXBF_GIGE_CONFIG_MAX_PKT_SZ_SHIFT);
	dev_dbg(&pdev->dev, "MLXBF_GIGE_CONFIG equals %llx - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readq(priv->base + MLXBF_GIGE_RX_WQE_SIZE_LOG2);
	exp_data = MLXBF_GIGE_RX_WQE_SIZE_LOG2_RESET_VAL;
	dev_dbg(&pdev->dev, "MLXBF_GIGE_RX_WQE_SIZE_LOG2 equals %llx - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Read data should match reset value in register header file */
	rd_data = readq(priv->base + MLXBF_GIGE_TX_WQ_SIZE_LOG2);
	exp_data = MLXBF_GIGE_TX_WQ_SIZE_LOG2_RESET_VAL;
	dev_dbg(&pdev->dev, "MLXBF_GIGE_TX_WQ_SIZE_LOG2 equals %llx - %s\n",
		rd_data, (rd_data == exp_data) ? "OK" : "FAIL");

	/* Do some SCRATCHPAD testing */
	rd_data = readq(priv->base + MLXBF_GIGE_SCRATCHPAD);
	dev_dbg(&pdev->dev, "MLXBF_GIGE_SCRATCHPAD equals %llx\n", rd_data);

	wr_data = 0x1122334455667788;
	writeq(wr_data, priv->base + MLXBF_GIGE_SCRATCHPAD);
	dev_dbg(&pdev->dev, "Will write %llx to MLXBF_GIGE_SCRATCHPAD\n",
		wr_data);

	rd_data = readq(priv->base + MLXBF_GIGE_SCRATCHPAD);
	dev_dbg(&pdev->dev, "MLXBF_GIGE_SCRATCHPAD equals %llx - %s\n",
		rd_data, (rd_data == wr_data) ? "OK" : "FAIL");

	wr_data = 0xaabbccddeeff4321;
	writeq(wr_data, priv->base + MLXBF_GIGE_SCRATCHPAD);
	dev_dbg(&pdev->dev, "Will write %llx to MLXBF_GIGE_SCRATCHPAD\n",
		wr_data);

	rd_data = readq(priv->base + MLXBF_GIGE_SCRATCHPAD);
	dev_dbg(&pdev->dev, "MLXBF_GIGE_SCRATCHPAD equals %llx - %s\n",
		rd_data, (rd_data == wr_data) ? "OK" : "FAIL");
#endif /* MLXBF_GIGE_MMIO_TESTS */
}
#endif /* MLXBF_GIGE_INTERNAL */

static void mlxbf_gige_set_mac_rx_filter(struct mlxbf_gige *priv,
					 unsigned int index, u64 dmac)
{
	void __iomem *base = priv->base;
	u64 control;

#ifdef MLXBF_GIGE_INTERNAL
	if (index > 3) {
		dev_err(priv->dev, "%s: invalid index %d\n",
			__func__, index);
		return;
	}

	dev_dbg(priv->dev, "set_mac_rx_filter: index=%d dmac=%llx\n",
		index, dmac);
#endif

	/* Write destination MAC to specified MAC RX filter */
	writeq(dmac, base + MLXBF_GIGE_RX_MAC_FILTER +
	       (index * MLXBF_GIGE_RX_MAC_FILTER_STRIDE));

	/* Enable MAC receive filter mask for specified index */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control |= (MLXBF_GIGE_CONTROL_EN_SPECIFIC_MAC << index);
	writeq(control, base + MLXBF_GIGE_CONTROL);
}

static int mlxbf_gige_get_mac_rx_filter(struct mlxbf_gige *priv,
					unsigned int index, u64 *dmac)
{
	void __iomem *base = priv->base;

#ifdef MLXBF_GIGE_INTERNAL
	if (index > 3) {
		dev_err(priv->dev, "%s: invalid index %d\n",
			__func__, index);
		return -EINVAL;
	}

	if (!dmac) {
		dev_err(priv->dev, "%s: invalid dmac pointer NULL\n",
			__func__);
		return -EINVAL;
	}
#endif

	/* Read destination MAC from specified MAC RX filter */
	*dmac = readq(base + MLXBF_GIGE_RX_MAC_FILTER +
		      (index * MLXBF_GIGE_RX_MAC_FILTER_STRIDE));

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(priv->dev, "get_mac_rx_filter: index=%d dmac=%llx\n",
		index, *dmac);
#endif

	return 0;
}

static void mlxbf_gige_enable_promisc(struct mlxbf_gige *priv)
{
	void __iomem *base = priv->base;
	u64 control;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(priv->dev, "%s\n", __func__);
#endif

	/* Enable MAC_ID_RANGE match functionality */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control |= MLXBF_GIGE_CONTROL_MAC_ID_RANGE_EN;
	writeq(control, base + MLXBF_GIGE_CONTROL);

	/* Set start of destination MAC range check to 0 */
	writeq(0, base + MLXBF_GIGE_RX_MAC_FILTER_DMAC_RANGE_START);

	/* Set end of destination MAC range check to all FFs */
	writeq(0xFFFFFFFFFFFF, base + MLXBF_GIGE_RX_MAC_FILTER_DMAC_RANGE_END);
}

static void mlxbf_gige_disable_promisc(struct mlxbf_gige *priv)
{
	void __iomem *base = priv->base;
	u64 control;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(priv->dev, "%s\n", __func__);
#endif

	/* Disable MAC_ID_RANGE match functionality */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control &= ~MLXBF_GIGE_CONTROL_MAC_ID_RANGE_EN;
	writeq(control, base + MLXBF_GIGE_CONTROL);

	/* NOTE: no need to change DMAC_RANGE_START or END;
	 * those values are ignored since MAC_ID_RANGE_EN=0
	 */
}

/* Receive Initialization
 * 1) Configures RX MAC filters via MMIO registers
 * 2) Allocates RX WQE array using coherent DMA mapping
 * 3) Initializes each element of RX WQE array with a receive
 *    buffer pointer (also using coherent DMA mapping)
 * 4) Allocates RX CQE array using coherent DMA mapping
 * 5) Completes other misc receive initialization
 */
static int mlxbf_gige_rx_init(struct mlxbf_gige *priv)
{
	size_t wq_size, cq_size;
	dma_addr_t *rx_wqe_ptr;
	dma_addr_t rx_buf_dma;
	u64 data;
	int i, j;

	/* Configure MAC RX filter #0 to allow RX of broadcast pkts */
	mlxbf_gige_set_mac_rx_filter(priv, MLXBF_GIGE_BCAST_MAC_FILTER_IDX,
				     BCAST_MAC_ADDR);

	wq_size = MLXBF_GIGE_RX_WQE_SZ * priv->rx_q_entries;
	priv->rx_wqe_base = dma_alloc_coherent(priv->dev, wq_size,
					       &priv->rx_wqe_base_dma,
					       GFP_KERNEL);
	if (!priv->rx_wqe_base)
		return -ENOMEM;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(priv->dev, "rx_init: RX WQE base 0x%llx 0x%llx\n",
		(u64)priv->rx_wqe_base, (u64)priv->rx_wqe_base_dma);
#endif

	/* Initialize 'rx_wqe_ptr' to point to first RX WQE in array
	 * Each RX WQE is simply a receive buffer pointer, so walk
	 * the entire array, allocating a 2KB buffer for each element
	 */
	rx_wqe_ptr = priv->rx_wqe_base;

	for (i = 0; i < priv->rx_q_entries; i++) {
#ifdef MLXBF_GIGE_INTERNAL
		u8 *p;
#endif
		/* Allocate a receive buffer for this RX WQE. The DMA
		 * form (dma_addr_t) of the receive buffer address is
		 * stored in the RX WQE array (via 'rx_wqe_ptr') where
		 * it is accessible by the GigE device. The VA form of
		 * the receive buffer is stored in 'rx_buf[]' array in
		 * the driver private storage for housekeeping.
		 */
		priv->rx_buf[i] = dma_alloc_coherent(priv->dev,
						     MLXBF_GIGE_DEFAULT_BUF_SZ,
						     &rx_buf_dma,
						     GFP_KERNEL);
		if (!priv->rx_buf[i])
			goto free_wqe_and_buf;

#ifdef MLXBF_GIGE_INTERNAL
		/* Initialize the first 16 bytes of each RX buffer
		 * to a known pattern. This will make it easy to
		 * identify when each receive buffer is populated
		 */
		p = priv->rx_buf[i];
		memset(p, MLXBF_GIGE_INIT_BYTE_RX_BUF, 16);
#endif
		*rx_wqe_ptr++ = rx_buf_dma;
	}

	/* Write RX WQE base address into MMIO reg */
	writeq(priv->rx_wqe_base_dma, priv->base + MLXBF_GIGE_RX_WQ_BASE);

	cq_size = MLXBF_GIGE_RX_CQE_SZ * priv->rx_q_entries;
	priv->rx_cqe_base = dma_alloc_coherent(priv->dev, cq_size,
					       &priv->rx_cqe_base_dma,
					       GFP_KERNEL);
	if (!priv->rx_cqe_base)
		goto free_wqe_and_buf;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(priv->dev, "rx_init: RX CQE base 0x%llx 0x%llx\n",
		(u64)priv->rx_cqe_base, (u64)priv->rx_cqe_base_dma);
#endif

	/* Write RX CQE base address into MMIO reg */
	writeq(priv->rx_cqe_base_dma, priv->base + MLXBF_GIGE_RX_CQ_BASE);

	/* Write RX_WQE_PI with current number of replenished buffers */
	writeq(priv->rx_q_entries, priv->base + MLXBF_GIGE_RX_WQE_PI);

	/* Enable RX DMA to write new packets to memory */
	writeq(MLXBF_GIGE_RX_DMA_EN, priv->base + MLXBF_GIGE_RX_DMA);

	/* Enable removal of CRC during RX */
	data = readq(priv->base + MLXBF_GIGE_RX);
	data |= MLXBF_GIGE_RX_STRIP_CRC_EN;
	writeq(data, priv->base + MLXBF_GIGE_RX);

	/* Enable RX MAC filter pass and discard counters */
	writeq(MLXBF_GIGE_RX_MAC_FILTER_COUNT_DISC_EN,
	       priv->base + MLXBF_GIGE_RX_MAC_FILTER_COUNT_DISC);
	writeq(MLXBF_GIGE_RX_MAC_FILTER_COUNT_PASS_EN,
	       priv->base + MLXBF_GIGE_RX_MAC_FILTER_COUNT_PASS);

	/* Clear MLXBF_GIGE_INT_MASK 'receive pkt' bit to
	 * indicate readiness to receive pkts
	 */
	data = readq(priv->base + MLXBF_GIGE_INT_MASK);
	data &= ~MLXBF_GIGE_INT_MASK_RX_RECEIVE_PACKET;
	writeq(data, priv->base + MLXBF_GIGE_INT_MASK);

	writeq(ilog2(priv->rx_q_entries),
	       priv->base + MLXBF_GIGE_RX_WQE_SIZE_LOG2);

	return 0;

free_wqe_and_buf:
	rx_wqe_ptr = priv->rx_wqe_base;
	for (j = 0; j < i; j++) {
		dma_free_coherent(priv->dev, MLXBF_GIGE_DEFAULT_BUF_SZ,
				  priv->rx_buf[j], *rx_wqe_ptr);
		rx_wqe_ptr++;
	}
	dma_free_coherent(priv->dev, wq_size,
			  priv->rx_wqe_base, priv->rx_wqe_base_dma);
	return -ENOMEM;
}

/* Transmit Initialization
 * 1) Allocates TX WQE array using coherent DMA mapping
 * 2) Allocates TX completion counter using coherent DMA mapping
 */
static int mlxbf_gige_tx_init(struct mlxbf_gige *priv)
{
	size_t size;

	size = MLXBF_GIGE_TX_WQE_SZ * priv->tx_q_entries;
	priv->tx_wqe_base = dma_alloc_coherent(priv->dev, size,
					       &priv->tx_wqe_base_dma,
					       GFP_KERNEL);
	if (!priv->tx_wqe_base)
		return -ENOMEM;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(priv->dev, "tx_init: TX WQE base 0x%llx 0x%llx\n",
		(u64)priv->tx_wqe_base, (u64)priv->tx_wqe_base_dma);
#endif

	priv->tx_wqe_next = priv->tx_wqe_base;

	/* Write TX WQE base address into MMIO reg */
	writeq(priv->tx_wqe_base_dma, priv->base + MLXBF_GIGE_TX_WQ_BASE);

	/* Allocate address for TX completion count */
	priv->tx_cc = dma_alloc_coherent(priv->dev, MLXBF_GIGE_TX_CC_SZ,
					 &priv->tx_cc_dma, GFP_KERNEL);

	if (!priv->tx_cc) {
		dma_free_coherent(priv->dev, size,
				  priv->tx_wqe_base, priv->tx_wqe_base_dma);
		return -ENOMEM;
	}

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(priv->dev, "tx_init: TX CC 0x%llx 0x%llx\n",
		(u64)priv->tx_cc, (u64)priv->tx_cc_dma);
#endif

	/* Write TX CC base address into MMIO reg */
	writeq(priv->tx_cc_dma, priv->base + MLXBF_GIGE_TX_CI_UPDATE_ADDRESS);

	writeq(ilog2(priv->tx_q_entries),
	       priv->base + MLXBF_GIGE_TX_WQ_SIZE_LOG2);

	priv->prev_tx_ci = 0;
	priv->tx_pi = 0;

	return 0;
}

/* Receive Deinitialization
 * This routine will free allocations done by mlxbf_gige_rx_init(),
 * namely the RX WQE and RX CQE arrays, as well as all RX buffers
 */
static void mlxbf_gige_rx_deinit(struct mlxbf_gige *priv)
{
	dma_addr_t *rx_wqe_ptr;
	size_t size;
	int i;

	rx_wqe_ptr = priv->rx_wqe_base;

	for (i = 0; i < priv->rx_q_entries; i++) {
		dma_free_coherent(priv->dev, MLXBF_GIGE_DEFAULT_BUF_SZ,
				  priv->rx_buf[i], *rx_wqe_ptr);
		priv->rx_buf[i] = NULL;
		rx_wqe_ptr++;
	}

	size = MLXBF_GIGE_RX_WQE_SZ * priv->rx_q_entries;
	dma_free_coherent(priv->dev, size,
			  priv->rx_wqe_base, priv->rx_wqe_base_dma);

	size = MLXBF_GIGE_RX_CQE_SZ * priv->rx_q_entries;
	dma_free_coherent(priv->dev, size,
			  priv->rx_cqe_base, priv->rx_cqe_base_dma);

	priv->rx_wqe_base = 0;
	priv->rx_wqe_base_dma = 0;
	priv->rx_cqe_base = 0;
	priv->rx_cqe_base_dma = 0;
	writeq(0, priv->base + MLXBF_GIGE_RX_WQ_BASE);
	writeq(0, priv->base + MLXBF_GIGE_RX_CQ_BASE);
}

/* Transmit Deinitialization
 * This routine will free allocations done by mlxbf_gige_tx_init(),
 * namely the TX WQE array and the TX completion counter
 */
static void mlxbf_gige_tx_deinit(struct mlxbf_gige *priv)
{
	u64 *tx_wqe_ptr;
	size_t size;
	int i;

	tx_wqe_ptr = priv->tx_wqe_base;

	for (i = 0; i < priv->tx_q_entries; i++) {
		if (priv->tx_buf[i]) {
			dma_free_coherent(priv->dev, MLXBF_GIGE_DEFAULT_BUF_SZ,
					  priv->tx_buf[i], *tx_wqe_ptr);
			priv->tx_buf[i] = NULL;
		}
		tx_wqe_ptr += 2;
	}

	size = MLXBF_GIGE_TX_WQE_SZ * priv->tx_q_entries;
	dma_free_coherent(priv->dev, size,
			  priv->tx_wqe_base, priv->tx_wqe_base_dma);

	dma_free_coherent(priv->dev, MLXBF_GIGE_TX_CC_SZ,
			  priv->tx_cc, priv->tx_cc_dma);

	priv->tx_wqe_base = 0;
	priv->tx_wqe_base_dma = 0;
	priv->tx_cc = 0;
	priv->tx_cc_dma = 0;
	priv->tx_wqe_next = 0;
	writeq(0, priv->base + MLXBF_GIGE_TX_WQ_BASE);
	writeq(0, priv->base + MLXBF_GIGE_TX_CI_UPDATE_ADDRESS);
}

/* Start of struct ethtool_ops functions */
static int mlxbf_gige_get_regs_len(struct net_device *netdev)
{
	/* Return size of MMIO register space (in bytes).
	 *
	 * NOTE: MLXBF_GIGE_MAC_CFG is the last defined register offset,
	 * so use that plus size of single register to derive total size
	 */
	return MLXBF_GIGE_MAC_CFG + 8;
}

static void mlxbf_gige_get_regs(struct net_device *netdev,
				struct ethtool_regs *regs, void *p)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);
	u64 *buff = p;
	int reg;

	regs->version = MLXBF_GIGE_REGS_VERSION;

	/* Read entire MMIO register space and store results
	 * into the provided buffer. Each 64-bit word is converted
	 * to big-endian to make the output more readable.
	 *
	 * NOTE: by design, a read to an offset without an existing
	 *       register will be acknowledged and return zero.
	 */
	for (reg = 0; reg <= MLXBF_GIGE_MAC_CFG; reg += 8)
		*buff++ = cpu_to_be64(readq(priv->base + reg));
}

static void mlxbf_gige_get_ringparam(struct net_device *netdev,
				     struct ethtool_ringparam *ering)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);

	memset(ering, 0, sizeof(*ering));
	ering->rx_max_pending = MLXBF_GIGE_MAX_RXQ_SZ;
	ering->tx_max_pending = MLXBF_GIGE_MAX_TXQ_SZ;
	ering->rx_pending = priv->rx_q_entries;
	ering->tx_pending = priv->tx_q_entries;
}

static int mlxbf_gige_set_ringparam(struct net_device *netdev,
				    struct ethtool_ringparam *ering)
{
	const struct net_device_ops *ops = netdev->netdev_ops;
	struct mlxbf_gige *priv = netdev_priv(netdev);
	int new_rx_q_entries, new_tx_q_entries;

	/* Device does not have separate queues for small/large frames */
	if (ering->rx_mini_pending || ering->rx_jumbo_pending)
		return -EINVAL;

	/* Round up to supported values */
	new_rx_q_entries = roundup_pow_of_two(ering->rx_pending);
	new_tx_q_entries = roundup_pow_of_two(ering->tx_pending);

	/* Range check the new values */
	if (new_tx_q_entries < MLXBF_GIGE_MIN_TXQ_SZ ||
	    new_tx_q_entries > MLXBF_GIGE_MAX_TXQ_SZ ||
	    new_rx_q_entries < MLXBF_GIGE_MIN_RXQ_SZ ||
	    new_rx_q_entries > MLXBF_GIGE_MAX_RXQ_SZ)
		return -EINVAL;

	/* If queue sizes did not change, exit now */
	if (new_rx_q_entries == priv->rx_q_entries &&
	    new_tx_q_entries == priv->tx_q_entries)
		return 0;

#ifdef MLXBF_GIGE_INTERNAL
	netdev_printk(KERN_DEBUG, netdev,
		      "set_ringparam(): new_tx=%x new_rx=%x\n",
		      new_tx_q_entries, new_rx_q_entries);
#endif

	if (netif_running(netdev))
		ops->ndo_stop(netdev);

	priv->rx_q_entries = new_rx_q_entries;
	priv->tx_q_entries = new_tx_q_entries;

	if (netif_running(netdev))
		ops->ndo_open(netdev);

	return 0;
}

static void mlxbf_gige_get_drvinfo(struct net_device *netdev,
				   struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, dev_name(&netdev->dev), sizeof(info->bus_info));
}

static const struct {
	const char string[ETH_GSTRING_LEN];
} mlxbf_gige_ethtool_stats_keys[] = {
	{ "rx_bytes" },
	{ "rx_packets" },
	{ "tx_bytes" },
	{ "tx_packets" },
	{ "hw_access_errors" },
	{ "tx_invalid_checksums" },
	{ "tx_small_frames" },
	{ "tx_index_errors" },
	{ "sw_config_errors" },
	{ "sw_access_errors" },
	{ "rx_truncate_errors" },
	{ "rx_mac_errors" },
	{ "rx_din_dropped_pkts" },
	{ "tx_fifo_full" },
	{ "rx_filter_passed_pkts" },
	{ "rx_filter_discard_pkts" },
};

static int mlxbf_gige_get_sset_count(struct net_device *netdev, int stringset)
{
	if (stringset != ETH_SS_STATS)
		return -EOPNOTSUPP;
	return ARRAY_SIZE(mlxbf_gige_ethtool_stats_keys);
}

static void mlxbf_gige_get_strings(struct net_device *netdev, u32 stringset,
				   u8 *buf)
{
	if (stringset != ETH_SS_STATS)
		return;
	memcpy(buf, &mlxbf_gige_ethtool_stats_keys,
	       sizeof(mlxbf_gige_ethtool_stats_keys));
}

static void mlxbf_gige_get_ethtool_stats(struct net_device *netdev,
					 struct ethtool_stats *estats,
					 u64 *data)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);

	/* Fill data array with interface statistics
	 *
	 * NOTE: the data writes must be in
	 *       sync with the strings shown in
	 *       the mlxbf_gige_ethtool_stats_keys[] array
	 *
	 * NOTE2: certain statistics below are zeroed upon
	 *        port disable, so the calculation below
	 *        must include the "cached" value of the stat
	 *        plus the value read directly from hardware.
	 *        Cached statistics are currently:
	 *          rx_din_dropped_pkts
	 *          rx_filter_passed_pkts
	 *          rx_filter_discard_pkts
	 */
	*data++ = netdev->stats.rx_bytes;
	*data++ = netdev->stats.rx_packets;
	*data++ = netdev->stats.tx_bytes;
	*data++ = netdev->stats.tx_packets;
	*data++ = priv->stats.hw_access_errors;
	*data++ = priv->stats.tx_invalid_checksums;
	*data++ = priv->stats.tx_small_frames;
	*data++ = priv->stats.tx_index_errors;
	*data++ = priv->stats.sw_config_errors;
	*data++ = priv->stats.sw_access_errors;
	*data++ = priv->stats.rx_truncate_errors;
	*data++ = priv->stats.rx_mac_errors;
	*data++ = (priv->stats.rx_din_dropped_pkts +
		   readq(priv->base + MLXBF_GIGE_RX_DIN_DROP_COUNTER));
	*data++ = priv->stats.tx_fifo_full;
	*data++ = (priv->stats.rx_filter_passed_pkts +
		   readq(priv->base + MLXBF_GIGE_RX_PASS_COUNTER_ALL));
	*data++ = (priv->stats.rx_filter_discard_pkts +
		   readq(priv->base + MLXBF_GIGE_RX_DISC_COUNTER_ALL));
}

static const struct ethtool_ops mlxbf_gige_ethtool_ops = {
	.get_drvinfo		= mlxbf_gige_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_ringparam		= mlxbf_gige_get_ringparam,
	.set_ringparam		= mlxbf_gige_set_ringparam,
	.get_regs_len           = mlxbf_gige_get_regs_len,
	.get_regs               = mlxbf_gige_get_regs,
	.get_strings            = mlxbf_gige_get_strings,
	.get_sset_count         = mlxbf_gige_get_sset_count,
	.get_ethtool_stats      = mlxbf_gige_get_ethtool_stats,
};

#ifdef FAST_MODELS
static void mlxbf_gige_handle_link_change(struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);
	struct phy_device *phydev = netdev->phydev;
	irqreturn_t ret;

	ret = mlxbf_gige_mdio_handle_phy_interrupt(priv);
	if (ret != IRQ_HANDLED)
		return;

	/* print new link status only if the interrupt came from the PHY */
	phy_print_status(phydev);
}
#endif /* FAST_MODELS */

/* Start of struct net_device_ops functions */
static irqreturn_t mlxbf_gige_error_intr(int irq, void *dev_id)
{
	struct mlxbf_gige *priv;
	u64 int_status;

	priv = dev_id;

	priv->error_intr_count++;

	int_status = readq(priv->base + MLXBF_GIGE_INT_STATUS);

#ifdef MLXBF_GIGE_INTERNAL
	/* Trigger kernel log message on first interrupt of each
	 * type and then service the asserted error condition(s).
	 */
#endif

	if (int_status & MLXBF_GIGE_INT_STATUS_HW_ACCESS_ERROR) {
		priv->stats.hw_access_errors++;
#ifdef MLXBF_GIGE_INTERNAL
		if (priv->stats.hw_access_errors == 1) {
			dev_info(priv->dev,
				 "%s: hw_access_error triggered\n",
				 __func__);
		}
		/* TODO - add logic to service hw_access_error */
#endif
	}

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_CHECKSUM_INPUTS) {
		priv->stats.tx_invalid_checksums++;
#ifdef MLXBF_GIGE_INTERNAL
		if (priv->stats.tx_invalid_checksums == 1) {
			dev_info(priv->dev,
				 "%s: tx_invalid_checksum triggered\n",
				 __func__);
		}
#endif
		/* This error condition is latched into MLXBF_GIGE_INT_STATUS
		 * when the GigE silicon operates on the offending
		 * TX WQE. The write to MLXBF_GIGE_INT_STATUS at the bottom
		 * of this routine clears this error condition.
		 */
	}

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_SMALL_FRAME_SIZE) {
		priv->stats.tx_small_frames++;
		/* This condition happens when the networking stack invokes
		 * this driver's "start_xmit()" method with a packet whose
		 * size < 60 bytes.  The GigE silicon will automatically pad
		 * this small frame up to a minimum-sized frame before it is
		 * sent. The "tx_small_frame" condition is latched into the
		 * MLXBF_GIGE_INT_STATUS register when the GigE silicon
		 * operates on the offending TX WQE. The write to
		 * MLXBF_GIGE_INT_STATUS at the bottom of this routine
		 * clears this condition.
		 */
	}

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_PI_CI_EXCEED_WQ_SIZE) {
		priv->stats.tx_index_errors++;
#ifdef MLXBF_GIGE_INTERNAL
		if (priv->stats.tx_index_errors == 1) {
			dev_info(priv->dev,
				 "%s: tx_index_error triggered\n",
				 __func__);
		}
		/* TODO - add logic to service tx_index_error */
#endif
	}

	if (int_status & MLXBF_GIGE_INT_STATUS_SW_CONFIG_ERROR) {
		priv->stats.sw_config_errors++;
#ifdef MLXBF_GIGE_INTERNAL
		if (priv->stats.sw_config_errors == 1) {
			dev_info(priv->dev,
				 "%s: sw_config_error triggered\n",
				 __func__);
		}
		/* TODO - add logic to service sw_config_error */
#endif
	}

	if (int_status & MLXBF_GIGE_INT_STATUS_SW_ACCESS_ERROR) {
		priv->stats.sw_access_errors++;
#ifdef MLXBF_GIGE_INTERNAL
		if (priv->stats.sw_access_errors == 1) {
			dev_info(priv->dev,
				 "%s: sw_access_error triggered\n",
				 __func__);
		}
		/* TODO - add logic to service sw_access_error */
#endif
	}

	/* Clear all error interrupts by writing '1' back to
	 * all the asserted bits in INT_STATUS.  Do not write
	 * '1' back to 'receive packet' bit, since that is
	 * managed separately.
	 */

	int_status &= ~MLXBF_GIGE_INT_STATUS_RX_RECEIVE_PACKET;

	writeq(int_status, priv->base + MLXBF_GIGE_INT_STATUS);

	return IRQ_HANDLED;
}

static irqreturn_t mlxbf_gige_rx_intr(int irq, void *dev_id)
{
	struct mlxbf_gige *priv;

	priv = dev_id;

	priv->rx_intr_count++;

	/* Driver has been interrupted because a new packet is available,
	 * but do not process packets at this time.  Instead, disable any
	 * further "packet rx" interrupts and tell the networking subsystem
	 * to poll the driver to pick up all available packets.
	 *
	 * NOTE: GigE silicon automatically disables "packet rx" interrupt by
	 *       setting MLXBF_GIGE_INT_MASK bit0 upon triggering the interrupt
	 *       to the ARM cores.  Software needs to re-enable "packet rx"
	 *       interrupts by clearing MLXBF_GIGE_INT_MASK bit0.
	 */

	/* Tell networking subsystem to poll GigE driver */
	napi_schedule(&priv->napi);

	return IRQ_HANDLED;
}

static irqreturn_t mlxbf_gige_llu_plu_intr(int irq, void *dev_id)
{
	struct mlxbf_gige *priv;

	priv = dev_id;
	priv->llu_plu_intr_count++;

#ifdef MLXBF_GIGE_INTERNAL
	/* Trigger kernel log message on first interrupt */
	if (priv->llu_plu_intr_count == 1)
		dev_info(priv->dev, "%s: triggered\n", __func__);

	/* TODO - add logic to service LLU and PLU interrupts */
#endif

	return IRQ_HANDLED;
}

/* Function that returns status of TX ring:
 *          0: TX ring is full, i.e. there are no
 *             available un-used entries in TX ring.
 *   non-null: TX ring is not full, i.e. there are
 *             some available entries in TX ring.
 *             The non-null value is a measure of
 *             how many TX entries are available, but
 *             it is not the exact number of available
 *             entries (see below).
 *
 * The algorithm makes the assumption that if
 * (prev_tx_ci == tx_pi) then the TX ring is empty.
 * An empty ring actually has (tx_q_entries-1)
 * entries, which allows the algorithm to differentiate
 * the case of an empty ring vs. a full ring.
 */
static u16 mlxbf_gige_tx_buffs_avail(struct mlxbf_gige *priv)
{
	u16 avail;

	if (priv->prev_tx_ci == priv->tx_pi)
		avail = priv->tx_q_entries - 1;
	else
		avail = (((priv->tx_q_entries + priv->prev_tx_ci - priv->tx_pi)
			  % priv->tx_q_entries) - 1);
	return avail;
}

static bool mlxbf_gige_handle_tx_complete(struct mlxbf_gige *priv)
{
	struct net_device_stats *stats;
	u16 tx_wqe_index;
	u64 *tx_wqe_addr;
	u64 tx_status;
	u16 tx_ci;

	tx_status = readq(priv->base + MLXBF_GIGE_TX_STATUS);
	if (tx_status & MLXBF_GIGE_TX_STATUS_DATA_FIFO_FULL)
		priv->stats.tx_fifo_full++;
	tx_ci = readq(priv->base + MLXBF_GIGE_TX_CONSUMER_INDEX);
	stats = &priv->netdev->stats;

	/* Transmit completion logic needs to loop until the completion
	 * index (in SW) equals TX consumer index (from HW).  These
	 * parameters are unsigned 16-bit values and the wrap case needs
	 * to be supported, that is TX consumer index wrapped from 0xFFFF
	 * to 0 while TX completion index is still < 0xFFFF.
	 */
	for (; priv->prev_tx_ci != tx_ci; priv->prev_tx_ci++) {
		tx_wqe_index = priv->prev_tx_ci % priv->tx_q_entries;
		/* Each TX WQE is 16 bytes. The 8 MSB store the 2KB TX
		 * buffer address and the 8 LSB contain information
		 * about the TX WQE.
		 */
		tx_wqe_addr = (priv->tx_wqe_base +
			       (tx_wqe_index * MLXBF_GIGE_TX_WQE_SZ_QWORDS));

		stats->tx_packets++;
		stats->tx_bytes += MLXBF_GIGE_TX_WQE_PKT_LEN(tx_wqe_addr);
#ifndef MLXBF_GIGE_LOOPBACK
		dma_free_coherent(priv->dev, MLXBF_GIGE_DEFAULT_BUF_SZ,
				  priv->tx_buf[tx_wqe_index], *tx_wqe_addr);
		priv->tx_buf[tx_wqe_index] = NULL;
#endif
	}

	/* Since the TX ring was likely just drained, check if TX queue
	 * had previously been stopped and now that there are TX buffers
	 * available the TX queue can be awakened.
	 */
	if (netif_queue_stopped(priv->netdev) &&
	    mlxbf_gige_tx_buffs_avail(priv)) {
#ifdef MLXBF_GIGE_INTERNAL
		dev_dbg(priv->dev, "%s: waking TX queue", __func__);
#endif
		netif_wake_queue(priv->netdev);
	}

	return true;
}

static bool mlxbf_gige_rx_packet(struct mlxbf_gige *priv, int *rx_pkts)
{
	struct net_device *netdev = priv->netdev;
	u16 rx_pi_rem, rx_ci_rem;
	struct sk_buff *skb;
	u64 *rx_cqe_addr;
	u64 datalen;
	u64 rx_cqe;
	u16 rx_ci;
	u16 rx_pi;
	u8 *pktp;

	/* Index into RX buffer array is rx_pi w/wrap based on RX_CQE_SIZE */
	rx_pi = readq(priv->base + MLXBF_GIGE_RX_WQE_PI);
	rx_pi_rem = rx_pi % priv->rx_q_entries;
	pktp = priv->rx_buf[rx_pi_rem];
	rx_cqe_addr = priv->rx_cqe_base + rx_pi_rem;
	rx_cqe = *rx_cqe_addr;
	datalen = rx_cqe & MLXBF_GIGE_RX_CQE_PKT_LEN_MASK;

	if ((rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MASK) == 0) {
		/* Packet is OK, increment stats */
		netdev->stats.rx_packets++;
		netdev->stats.rx_bytes += datalen;
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MAC_ERR) {
		priv->stats.rx_mac_errors++;
#ifdef MLXBF_GIGE_INTERNAL
		/* TODO - handle error case */
#endif
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_TRUNCATED) {
		priv->stats.rx_truncate_errors++;
#ifdef MLXBF_GIGE_INTERNAL
		/* TODO - handle error case */
#endif
	}

	skb = dev_alloc_skb(datalen);
	if (!skb) {
		netdev->stats.rx_dropped++;
		return false;
	}

	memcpy(skb_put(skb, datalen), pktp, datalen);

	/* Let hardware know we've replenished one buffer */
	writeq(rx_pi + 1, priv->base + MLXBF_GIGE_RX_WQE_PI);

	skb->dev = netdev;
	skb->protocol = eth_type_trans(skb, netdev);
	skb->ip_summed = CHECKSUM_NONE; /* device did not checksum packet */

	netif_receive_skb(skb);
	(*rx_pkts)++;
	rx_pi = readq(priv->base + MLXBF_GIGE_RX_WQE_PI);
	rx_pi_rem = rx_pi % priv->rx_q_entries;
	rx_ci = readq(priv->base + MLXBF_GIGE_RX_CQE_PACKET_CI);
	rx_ci_rem = rx_ci % priv->rx_q_entries;

	return rx_pi_rem != rx_ci_rem;
}

/* Driver poll() function called by NAPI infrastructure */
static int mlxbf_gige_poll(struct napi_struct *napi, int budget)
{
	struct mlxbf_gige *priv;
	bool remaining_pkts;
	int work_done = 0;
	u64 data;

	priv = container_of(napi, struct mlxbf_gige, napi);

	mlxbf_gige_handle_tx_complete(priv);

	do {
		remaining_pkts = mlxbf_gige_rx_packet(priv, &work_done);
	} while (remaining_pkts && work_done < budget);

	/* If amount of work done < budget, turn off NAPI polling
	 * via napi_complete_done(napi, work_done) and then
	 * re-enable interrupts.
	 */
	if (work_done < budget && napi_complete_done(napi, work_done)) {
		/* Clear MLXBF_GIGE_INT_MASK 'receive pkt' bit to
		 * indicate receive readiness
		 */
		data = readq(priv->base + MLXBF_GIGE_INT_MASK);
		data &= ~MLXBF_GIGE_INT_MASK_RX_RECEIVE_PACKET;
		writeq(data, priv->base + MLXBF_GIGE_INT_MASK);
	}

	return work_done;
}

static int mlxbf_gige_request_irqs(struct mlxbf_gige *priv)
{
	int err;

	err = devm_request_irq(priv->dev, priv->error_irq,
			       mlxbf_gige_error_intr, 0, "mlxbf_gige_error",
			       priv);
	if (err) {
		dev_err(priv->dev, "Request error_irq failure\n");
		return err;
	}

	err = devm_request_irq(priv->dev, priv->rx_irq,
			       mlxbf_gige_rx_intr, 0, "mlxbf_gige_rx",
			       priv);
	if (err) {
		dev_err(priv->dev, "Request rx_irq failure\n");
		return err;
	}

	err = devm_request_irq(priv->dev, priv->llu_plu_irq,
			       mlxbf_gige_llu_plu_intr, 0, "mlxbf_gige_llu_plu",
			       priv);
	if (err) {
		dev_err(priv->dev, "Request llu_plu_irq failure\n");
		return err;
	}

	return 0;
}

static void mlxbf_gige_free_irqs(struct mlxbf_gige *priv)
{
	devm_free_irq(priv->dev, priv->error_irq, priv);
	devm_free_irq(priv->dev, priv->rx_irq, priv);
	devm_free_irq(priv->dev, priv->llu_plu_irq, priv);
}

static int mlxbf_gige_open(struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);
#ifdef FAST_MODELS
	struct phy_device *phydev;
#endif
	u64 int_en;
	int err;

#ifdef MLXBF_GIGE_INTERNAL
	netdev_printk(KERN_DEBUG, netdev, "open: priv=%llx\n",
		      (u64)priv);
#endif

	memset(&priv->stats, 0, sizeof(priv->stats));

	mlxbf_gige_rx_init(priv);
	mlxbf_gige_tx_init(priv);
	netif_napi_add(netdev, &priv->napi, mlxbf_gige_poll, NAPI_POLL_WEIGHT);
	napi_enable(&priv->napi);
	netif_start_queue(netdev);

	err = mlxbf_gige_request_irqs(priv);
	if (err)
		return err;

#ifdef FAST_MODELS
	phydev = phy_find_first(priv->mdiobus);
	if (!phydev)
		return -EIO;

	/* Sets netdev->phydev to phydev; which will eventually
	 * be used in ioctl calls.
	 */
	err = phy_connect_direct(netdev, phydev,
				 mlxbf_gige_handle_link_change,
				 PHY_INTERFACE_MODE_GMII);
	if (err) {
		netdev_err(netdev, "Could not attach to PHY\n");
		return err;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	/* MAC only supports 1000T full duplex mode */
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_1000baseT_Half_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_100baseT_Full_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_100baseT_Half_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_10baseT_Full_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_10baseT_Half_BIT);

	/* MAC supports symmetric flow control */
	phy_support_sym_pause(phydev);
#else
	/* MAC only supports 1000T full duplex mode */
	phydev->supported &= ~SUPPORTED_1000baseT_Half;
	phydev->supported &= ~SUPPORTED_100baseT_Full;
	phydev->supported &= ~SUPPORTED_100baseT_Half;
	phydev->supported &= ~SUPPORTED_10baseT_Full;
	phydev->supported &= ~SUPPORTED_10baseT_Half;

	/* MAC supports symmetric flow control */
	phydev->supported |= SUPPORTED_Pause;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0) */

	phy_start(phydev);
	err = phy_start_aneg(phydev);
	if (err < 0) {
		netdev_err(netdev, "phy_start_aneg failure: 0x%x\n", err);
		return err;
	}

#ifdef MLXBF_GIGE_INTERNAL
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	netdev_printk(KERN_DEBUG, netdev, "supported: %*pb, advertising: %*pb,"
			" speed: 0x%x, duplex: 0x%x, autoneg: 0x%x, pause: 0x%x,"
			" asym_pause: 0x%x\n",
			__ETHTOOL_LINK_MODE_MASK_NBITS, phydev->supported,
			__ETHTOOL_LINK_MODE_MASK_NBITS, phydev->advertising,
			phydev->speed, phydev->duplex, phydev->autoneg,
			phydev->pause, phydev->asym_pause);
#else
	netdev_printk(KERN_DEBUG, netdev, "supported: 0x%x, advertising: 0x%x,"
		      " speed: 0x%x, duplex: 0x%x, autoneg: 0x%x, pause: 0x%x,"
		      " asym_pause: 0x%x\n",
		      phydev->supported, phydev->advertising, phydev->speed,
		      phydev->duplex, phydev->autoneg, phydev->pause,
		      phydev->asym_pause);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0) */
#endif /* MLXBF_GIGE_INTERNAL */

	/* Display information about attached PHY device */
	phy_attached_info(phydev);

#endif /* FAST_MODELS */

	/* Set bits in INT_EN that we care about */
	int_en = MLXBF_GIGE_INT_EN_HW_ACCESS_ERROR |
		 MLXBF_GIGE_INT_EN_TX_CHECKSUM_INPUTS |
		 MLXBF_GIGE_INT_EN_TX_SMALL_FRAME_SIZE |
		 MLXBF_GIGE_INT_EN_TX_PI_CI_EXCEED_WQ_SIZE |
		 MLXBF_GIGE_INT_EN_SW_CONFIG_ERROR |
		 MLXBF_GIGE_INT_EN_SW_ACCESS_ERROR |
		 MLXBF_GIGE_INT_EN_RX_RECEIVE_PACKET;
	writeq(int_en, priv->base + MLXBF_GIGE_INT_EN);

	return 0;
}

static void mlxbf_gige_clean_port(struct mlxbf_gige *priv)
{
	struct mlxbf_gige_stats *p;
	u64 control, status;
	int cnt;

	/* Cache stats that will be cleared by clean port operation */
	p = &priv->stats;
	p->rx_din_dropped_pkts = readq(priv->base + MLXBF_GIGE_RX_DIN_DROP_COUNTER);
	p->rx_filter_passed_pkts = readq(priv->base + MLXBF_GIGE_RX_PASS_COUNTER_ALL);
	p->rx_filter_discard_pkts = readq(priv->base + MLXBF_GIGE_RX_DISC_COUNTER_ALL);

	/* Set the CLEAN_PORT_EN bit to trigger SW reset */
	control = readq(priv->base + MLXBF_GIGE_CONTROL);
	control |= MLXBF_GIGE_CONTROL_CLEAN_PORT_EN;
	writeq(control, priv->base + MLXBF_GIGE_CONTROL);

	/* Create memory barrier before reading status */
	wmb();

	/* Loop waiting for status ready bit to assert */
	cnt = 1000;
	do {
		status = readq(priv->base + MLXBF_GIGE_STATUS);
		if (status & MLXBF_GIGE_STATUS_READY)
			break;
		usleep_range(50, 100);
	} while (--cnt > 0);

	/* Clear the CLEAN_PORT_EN bit at end of this loop */
	control = readq(priv->base + MLXBF_GIGE_CONTROL);
	control &= ~MLXBF_GIGE_CONTROL_CLEAN_PORT_EN;
	writeq(control, priv->base + MLXBF_GIGE_CONTROL);
}

static int mlxbf_gige_stop(struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);

#ifdef MLXBF_GIGE_INTERNAL
	netdev_printk(KERN_DEBUG, netdev, "stop: priv=%llx\n",
		      (u64)priv);
#endif

	writeq(0, priv->base + MLXBF_GIGE_INT_EN);
	netif_stop_queue(netdev);
	napi_disable(&priv->napi);
	netif_napi_del(&priv->napi);
	mlxbf_gige_free_irqs(priv);

#ifdef FAST_MODELS
	phy_stop(netdev->phydev);
	phy_disconnect(netdev->phydev);
#endif /* FAST_MODELS */

	mlxbf_gige_rx_deinit(priv);
	mlxbf_gige_tx_deinit(priv);
	mlxbf_gige_clean_port(priv);

	return 0;
}

/* Function to advance the tx_wqe_next pointer to next TX WQE */
static void mlxbf_gige_update_tx_wqe_next(struct mlxbf_gige *priv)
{
	/* Advance tx_wqe_next pointer */
	priv->tx_wqe_next += MLXBF_GIGE_TX_WQE_SZ_QWORDS;

	/* Check if 'next' pointer is beyond end of TX ring */
	/* If so, set 'next' back to 'base' pointer of ring */
	if (priv->tx_wqe_next == (priv->tx_wqe_base +
				  (priv->tx_q_entries * MLXBF_GIGE_TX_WQE_SZ_QWORDS)))
		priv->tx_wqe_next = priv->tx_wqe_base;
}

static netdev_tx_t mlxbf_gige_start_xmit(struct sk_buff *skb,
					 struct net_device *netdev)
{
#ifndef MLXBF_GIGE_LOOPBACK
	struct mlxbf_gige *priv = netdev_priv(netdev);
	dma_addr_t tx_buf_dma;
	u8 *tx_buf = NULL;
	u64 *tx_wqe_addr;
	u64 word2;

	/* Check that there is room left in TX ring */
	if (!mlxbf_gige_tx_buffs_avail(priv)) {
		/* TX ring is full, inform stack but do not free SKB */
#ifdef MLXBF_GIGE_INTERNAL
		dev_dbg(priv->dev, "%s: TX ring is full, stopping TX queue\n",
			__func__);
#endif
		netif_stop_queue(netdev);
		netdev->stats.tx_dropped++;
		return NETDEV_TX_BUSY;
	}

	/* Allocate ptr for buffer */
	if (skb->len < MLXBF_GIGE_DEFAULT_BUF_SZ)
		tx_buf = dma_alloc_coherent(priv->dev, MLXBF_GIGE_DEFAULT_BUF_SZ,
					    &tx_buf_dma, GFP_KERNEL);

	if (!tx_buf) {
		/* Free incoming skb, could not alloc TX buffer */
		dev_kfree_skb(skb);
		netdev->stats.tx_dropped++;
		return NET_XMIT_DROP;
	}

	priv->tx_buf[priv->tx_pi % priv->tx_q_entries] = tx_buf;

	/* Copy data from skb to allocated TX buffer
	 *
	 * NOTE: GigE silicon will automatically pad up to
	 *       minimum packet length if needed.
	 */
	skb_copy_bits(skb, 0, tx_buf, skb->len);

	/* Get address of TX WQE */
	tx_wqe_addr = priv->tx_wqe_next;

	mlxbf_gige_update_tx_wqe_next(priv);

	/* Put PA of buffer address into first 64-bit word of TX WQE */
	*tx_wqe_addr = tx_buf_dma;

	/* Set TX WQE pkt_len appropriately */
	word2 = skb->len & MLXBF_GIGE_TX_WQE_PKT_LEN_MASK;

	/* Write entire 2nd word of TX WQE */
	*(tx_wqe_addr + 1) = word2;

	priv->tx_pi++;

	/* Create memory barrier before write to TX PI */
	wmb();

	writeq(priv->tx_pi, priv->base + MLXBF_GIGE_TX_PRODUCER_INDEX);
#endif

	/* Free incoming skb, contents already copied to HW */
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static int mlxbf_gige_do_ioctl(struct net_device *netdev,
			       struct ifreq *ifr, int cmd)
{
#ifdef FAST_MODELS
	if (!(netif_running(netdev)))
		return -EINVAL;

	return phy_mii_ioctl(netdev->phydev, ifr, cmd);
#else
	return 0;

#endif /* FAST_MODELS*/
}

#ifdef MLXBF_GIGE_INTERNAL
static void mlxbf_gige_tx_timeout(struct net_device *netdev)
{
	/* TODO - add TX timeout logic */
}
#endif

static void mlxbf_gige_set_rx_mode(struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);
	bool new_promisc_enabled;

#ifdef MLXBF_GIGE_INTERNAL
	netdev_printk(KERN_DEBUG, netdev, "set_rx_mode: priv=%llx flags=%x\n",
		      (u64)priv, netdev->flags);
#endif

	new_promisc_enabled = netdev->flags & IFF_PROMISC;

	/* Only write to the hardware registers if the new setting
	 * of promiscuous mode is different from the current one.
	 */
	if (new_promisc_enabled != priv->promisc_enabled) {
		priv->promisc_enabled = new_promisc_enabled;

		if (new_promisc_enabled) {
#ifdef MLXBF_GIGE_INTERNAL
			netdev_printk(KERN_DEBUG, netdev,
				      "set_rx_mode: enable promisc\n");
#endif
			mlxbf_gige_enable_promisc(priv);
		} else {
#ifdef MLXBF_GIGE_INTERNAL
			netdev_printk(KERN_DEBUG, netdev,
				      "set_rx_mode: disable promisc\n");
#endif
			mlxbf_gige_disable_promisc(priv);
		}
	}
}

static const struct net_device_ops mlxbf_gige_netdev_ops = {
	.ndo_open		= mlxbf_gige_open,
	.ndo_stop		= mlxbf_gige_stop,
	.ndo_start_xmit		= mlxbf_gige_start_xmit,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_do_ioctl		= mlxbf_gige_do_ioctl,
	.ndo_set_rx_mode        = mlxbf_gige_set_rx_mode,
#ifdef MLXBF_GIGE_INTERNAL
	.ndo_tx_timeout         = mlxbf_gige_tx_timeout,
#endif
};

#ifdef MLXBF_GIGE_INTERNAL
#ifdef MLXBF_GIGE_MMIO_SYSFS
static ssize_t mmio_read_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t len)
{
	struct mlxbf_gige *priv;
	u64 offset;
	int cnt;

	priv = dev_get_drvdata(dev);

	cnt = sscanf(buf, "%llx\n", &offset);

	if (cnt != 1) {
		dev_err(dev, "MMIO read: invalid arguments\n");
		return len;
	}

	/* Make sure offset is within MAC block and 8-byte aligned */
	if (offset <= MLXBF_GIGE_MAC_CFG &&
	    ((offset & 0x7) == 0)) {
		dev_err(dev,
			"MMIO read: offset=0x%llx data=0x%llx\n",
			offset, readq(priv->base + offset));
	} else {
		dev_err(dev,
			"MMIO read: invalid offset 0x%llx\n",
			offset);
	}

	return len;
}

static ssize_t mmio_write_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct mlxbf_gige *priv;
	u64 offset, data;
	int cnt;

	priv = dev_get_drvdata(dev);

	cnt = sscanf(buf, "%llx %llx\n", &offset, &data);

	if (cnt != 2) {
		dev_err(dev, "MMIO write: invalid arguments\n");
		return len;
	}

	/* Make sure offset is within MAC block and 8-byte aligned */
	if (offset <= MLXBF_GIGE_MAC_CFG &&
	    ((offset & 0x7) == 0)) {
		dev_err(dev,
			"MMIO write: offset=0x%llx data=0x%llx\n",
			offset, data);
		writeq(data, priv->base + offset);
	} else {
		dev_err(dev,
			"MMIO write: invalid offset 0x%llx\n",
			offset);
	}

	return len;
}

static ssize_t llu_mmio_read_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	struct mlxbf_gige *priv;
	u64 offset;
	int cnt;

	priv = dev_get_drvdata(dev);

	cnt = sscanf(buf, "%llx\n", &offset);

	if (cnt != 1) {
		dev_err(dev, "LLU MMIO read: invalid arguments\n");
		return len;
	}

	/* Make sure offset is within LLU and 4-byte aligned */
	if (offset <= MLXBF_GIGE_LLU_MAX_OFFSET &&
	    ((offset & 0x3) == 0)) {
		dev_err(dev,
			"LLU MMIO read: offset=0x%llx data=0x%x\n",
			offset, readl(priv->llu_base + offset));
	} else {
		dev_err(dev,
			"LLU MMIO read: invalid offset 0x%llx\n",
			offset);
	}

	return len;
}

static ssize_t llu_mmio_write_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	struct mlxbf_gige *priv;
	u64 offset;
	u32 data;
	int cnt;

	priv = dev_get_drvdata(dev);

	cnt = sscanf(buf, "%llx %x\n", &offset, &data);

	if (cnt != 2) {
		dev_err(dev, "LLU MMIO write: invalid arguments\n");
		return len;
	}

	/* Make sure offset is within LLU and 4-byte aligned */
	if (offset <= MLXBF_GIGE_LLU_MAX_OFFSET &&
	    ((offset & 0x3) == 0)) {
		dev_err(dev,
			"LLU MMIO write: offset=0x%llx data=0x%x\n",
			offset, data);
		writel(data, priv->llu_base + offset);
	} else {
		dev_err(dev,
			"LLU MMIO write: invalid offset 0x%llx\n",
			offset);
	}

	return len;
}

static ssize_t plu_mmio_read_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	struct mlxbf_gige *priv;
	u64 offset;
	int cnt;

	priv = dev_get_drvdata(dev);

	cnt = sscanf(buf, "%llx\n", &offset);

	if (cnt != 1) {
		dev_err(dev, "PLU MMIO read: invalid arguments\n");
		return len;
	}

	/* Make sure offset is within PLU and 4-byte aligned */
	if (offset <= MLXBF_GIGE_PLU_MAX_OFFSET &&
	    ((offset & 0x3) == 0)) {
		dev_err(dev,
			"PLU MMIO read: offset=0x%llx data=0x%x\n",
			offset, readl(priv->plu_base + offset));
	} else {
		dev_err(dev,
			"PLU MMIO read: invalid offset 0x%llx\n",
			offset);
	}

	return len;
}

static ssize_t plu_mmio_write_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	struct mlxbf_gige *priv;
	u64 offset;
	u32 data;
	int cnt;

	priv = dev_get_drvdata(dev);

	cnt = sscanf(buf, "%llx %x\n", &offset, &data);

	if (cnt != 2) {
		dev_err(dev, "PLU MMIO write: invalid arguments\n");
		return len;
	}

	/* Make sure offset is within PLU and 4-byte aligned */
	if (offset <= MLXBF_GIGE_PLU_MAX_OFFSET &&
	    ((offset & 0x3) == 0)) {
		dev_err(dev,
			"PLU MMIO write: offset=0x%llx data=0x%x\n",
			offset, data);
		writel(data, priv->plu_base + offset);
	} else {
		dev_err(dev,
			"PLU MMIO write: invalid offset 0x%llx\n",
			offset);
	}

	return len;
}

DEVICE_ATTR_WO(mmio_read);
DEVICE_ATTR_WO(mmio_write);
DEVICE_ATTR_WO(llu_mmio_read);
DEVICE_ATTR_WO(llu_mmio_write);
DEVICE_ATTR_WO(plu_mmio_read);
DEVICE_ATTR_WO(plu_mmio_write);
#endif /* MLXBF_GIGE_MMIO_SYSFS */

static void oob_dump_tx_wqe(struct device *dev, u64 *tx_wqe_addr)
{
	u64 word1, word2;

	/* Sanity check the TX WQE address */
	if (!tx_wqe_addr)
		return;

	word1 = *tx_wqe_addr;
	word2 = *(tx_wqe_addr + 1);

	/* If TX WQE is empty (i.e. both words are 0)
	 * then don't bother displaying WQE details
	 */
	if (word1 == (u64)0 && word2 == (u64)0) {
		dev_dbg(dev, "%s(%llx)=%llx %llx", __func__,
			(u64)tx_wqe_addr, word1, word2);
	} else {
		dev_dbg(dev, "%s(%llx)", __func__,
			(u64)tx_wqe_addr);

		dev_dbg(dev, "  buffer addr:  %llx\n", word1);

		dev_dbg(dev, "  pkt_len:      %llx\n",
			((word2 & MLXBF_GIGE_TX_WQE_PKT_LEN_MASK)
			 >> MLXBF_GIGE_TX_WQE_PKT_LEN_SHIFT));

		dev_dbg(dev, "  update:       %llx\n",
			((word2 & MLXBF_GIGE_TX_WQE_UPDATE_MASK)
			 >> MLXBF_GIGE_TX_WQE_UPDATE_SHIFT));

		dev_dbg(dev, "  cksum_len:    %llx\n",
			((word2 & MLXBF_GIGE_TX_WQE_CHKSUM_LEN_MASK)
			 >> MLXBF_GIGE_TX_WQE_CHKSUM_LEN_SHIFT));

		dev_dbg(dev, "  cksum_start:  %llx\n",
			((word2 & MLXBF_GIGE_TX_WQE_CHKSUM_START_MASK)
			 >> MLXBF_GIGE_TX_WQE_CHKSUM_START_SHIFT));

		dev_dbg(dev, "  cksum_offset: %llx\n",
			((word2 & MLXBF_GIGE_TX_WQE_CHKSUM_OFFSET_MASK)
			 >> MLXBF_GIGE_TX_WQE_CHKSUM_OFFSET_SHIFT));
	}
}

/* Handler for sysfs entry 'dump_tx' found at
 *   /sys/devices/platform/MLNXBF17:00/
 * Issue 'cat dump_tx' to invoke this routine
 */
static ssize_t dump_tx_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mlxbf_gige *priv;
	u64 data;
	int i;

	priv = dev_get_drvdata(dev);

	dev_dbg(dev, "============================================\n");

	dev_dbg(dev, "%s: dev_get_drvdata=%llx\n",
		__func__, (u64)priv);

	/* Loop through 'n' TX WQE entries */
	for (i = 0;
	     (priv->netdev->flags & IFF_UP) && (i < priv->tx_q_entries);
	     i++) {
		u8 *p;

		oob_dump_tx_wqe(dev,
				(u64 *)((u64)priv->tx_wqe_base +
				(i * MLXBF_GIGE_TX_WQE_SZ)));

		p = priv->tx_buf[i];
		dev_dbg(dev, "  tx_buf[%d] = %llx\n", i, (u64)priv->tx_buf[i]);

		if (p) {
			int j;

			for (j = 0;
			     j < (MLXBF_GIGE_NUM_BYTES_IN_PKT_DUMP / 16);
			     j++, p += 16) {
				dev_dbg(dev, MLXBF_GIGE_MSG_FORMAT,
					MLXBF_GIGE_MSG_ARGS(p));
			}
		}
	}

	if (priv->netdev->flags & IFF_UP) {
		dev_dbg(dev, "tx_cc=%llx *tx_cc=%llx\n",
			(u64)priv->tx_cc, *(u64 *)priv->tx_cc);
	}

	/* Display TX producer index */
	data = readq(priv->base + MLXBF_GIGE_TX_PRODUCER_INDEX);
	dev_dbg(dev, "tx_producer_index=%llx\n", (u64)data);

	/* Display TX consumer index */
	data = readq(priv->base + MLXBF_GIGE_TX_CONSUMER_INDEX);
	dev_dbg(dev, "tx_consumer_index=%llx\n", (u64)data);

	/* Display TX status */
	data = readq(priv->base + MLXBF_GIGE_TX_STATUS);
	dev_dbg(dev, "tx_status=%llx\n", (u64)data);

	/* Display TX FIFO status */
	data = readq(priv->base + MLXBF_GIGE_TX_FIFOS_STATUS);
	dev_dbg(dev, "tx_fifos_status=%llx\n", (u64)data);

	return strlen(buf);
}

DEVICE_ATTR_RO(dump_tx);

static void oob_dump_rx_wqe(struct device *dev, u64 *rx_wqe_addr)
{
	/* Sanity check the RX WQE address */
	if (!rx_wqe_addr)
		return;

	dev_dbg(dev, "%s(%llx)=%llx\n", __func__,
		(u64)rx_wqe_addr, *rx_wqe_addr);
}

static void oob_dump_rx_cqe(struct device *dev, u64 *rx_cqe_addr)
{
	u64 rx_cqe;

	/* Sanity check the RX CQE address */
	if (!rx_cqe_addr)
		return;

	rx_cqe = *rx_cqe_addr;

	/* If RX CQE is empty (i.e. value is 0) then
	 * don't bother displaying CQE details
	 */
	if (rx_cqe == (u64)0) {
		dev_dbg(dev, "%s(%llx)=%llx", __func__,
			(u64)rx_cqe_addr, rx_cqe);
	} else {
		dev_dbg(dev, "%s(%llx)", __func__,
			(u64)rx_cqe_addr);

		dev_dbg(dev, "  pkt_len:    %llx\n",
			(rx_cqe & MLXBF_GIGE_RX_CQE_PKT_LEN_MASK));

		dev_dbg(dev, "  valid:      %llx\n",
			((rx_cqe & MLXBF_GIGE_RX_CQE_VALID_MASK)
			 >> MLXBF_GIGE_RX_CQE_VALID_SHIFT));

		dev_dbg(dev, "  pkt_status: %llx\n",
			((rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MASK)
			 >> MLXBF_GIGE_RX_CQE_PKT_STATUS_SHIFT));

		dev_dbg(dev, "  chksum:     %llx\n",
			((rx_cqe & MLXBF_GIGE_RX_CQE_CHKSUM_MASK)
			 >> MLXBF_GIGE_RX_CQE_CHKSUM_SHIFT));
	}
}

/* Handler for sysfs entry 'dump_rx' found at
 *   /sys/devices/platform/MLNXBF17:00/
 * Issue 'cat dump_rx' to invoke this routine
 */
static ssize_t dump_rx_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct mlxbf_gige *priv;
	u64 data;
	int i;

	priv = dev_get_drvdata(dev);

	dev_dbg(dev, "============================================\n");
	dev_dbg(dev, "%s: dev_get_drvdata=%llx\n", __func__, (u64)priv);

	/* Loop through 'n' RX WQE entries */
	for (i = 0;
	     (priv->netdev->flags & IFF_UP) && (i < priv->rx_q_entries);
	     i++) {
		u8 *p;

		oob_dump_rx_wqe(dev, priv->rx_wqe_base + i);

		p = priv->rx_buf[i];
		dev_dbg(dev, "  rx_buf[%d] = %llx\n", i, (u64)priv->rx_buf[i]);

		/* Only display RX buffer contents if not in initial state */
		if (p && (*p != MLXBF_GIGE_INIT_BYTE_RX_BUF)) {
			int j;

			for (j = 0;
			     j < (MLXBF_GIGE_NUM_BYTES_IN_PKT_DUMP / 16);
			     j++, p += 16) {
				dev_dbg(dev, MLXBF_GIGE_MSG_FORMAT,
					MLXBF_GIGE_MSG_ARGS(p));
			}
		}
	}

	/* Loop through 'n' RX CQE entries */
	for (i = 0;
	     (priv->netdev->flags & IFF_UP) && (i < priv->rx_q_entries);
	     i++) {
		oob_dump_rx_cqe(dev, priv->rx_cqe_base + i);
	}

	/* Display RX WQE producer index */
	data = readq(priv->base + MLXBF_GIGE_RX_WQE_PI);
	dev_dbg(dev, "rx_wqe_pi=%llx\n", (u64)data);

	/* Display INT_STATUS */
	data = readq(priv->base + MLXBF_GIGE_INT_STATUS);
	dev_dbg(dev, "int_status=%llx\n", (u64)data);

	/* Then, clear INT_STATUS */
	data = 0x1FF;
	writeq(data, priv->base + MLXBF_GIGE_INT_STATUS);

	/* Display RX_DIN_DROP_COUNTER */
	data = readq(priv->base + MLXBF_GIGE_RX_DIN_DROP_COUNTER);
	dev_dbg(dev, "rx_din_drop_counter=%llx\n", (u64)data);

	/* Display INT_STATUS_EXP */
	data = readq(priv->base + MLXBF_GIGE_INT_STATUS_EXP);
	dev_dbg(dev, "int_status_exp=%llx\n", (u64)data);

	/* Then, clear INT_STATUS_EXP */
	data = 0;
	writeq(data, priv->base + MLXBF_GIGE_INT_STATUS_EXP);

	/* Display RX_MAC_FILTER_PASS_COUNTER_ALL */
	data = readq(priv->base + MLXBF_GIGE_RX_PASS_COUNTER_ALL);
	dev_dbg(dev, "rx_mac_filter_pass_counter_all=%llx\n", (u64)data);

	/* Display RX_MAC_FILTER_DISC_COUNTER_ALL */
	data = readq(priv->base + MLXBF_GIGE_RX_DISC_COUNTER_ALL);
	dev_dbg(dev, "rx_mac_filter_disc_counter_all=%llx\n", (u64)data);

	/* Display first word of RX_MAC_FILTER */
	data = readq(priv->base + MLXBF_GIGE_RX_MAC_FILTER);
	dev_dbg(dev, "rx_mac_filter0=%llx\n", (u64)data);

	/* Display second word of RX_MAC_FILTER */
	data = readq(priv->base + MLXBF_GIGE_RX_MAC_FILTER + 0x8);
	dev_dbg(dev, "rx_mac_filter1=%llx\n", (u64)data);

	/* Display third word of RX_MAC_FILTER */
	data = readq(priv->base + MLXBF_GIGE_RX_MAC_FILTER + 0x10);
	dev_dbg(dev, "rx_mac_filter2=%llx\n", (u64)data);

	/* Display fourth word of RX_MAC_FILTER */
	data = readq(priv->base + MLXBF_GIGE_RX_MAC_FILTER + 0x18);
	dev_dbg(dev, "rx_mac_filter3=%llx\n", (u64)data);

	/* Display MLXBF_GIGE_RX_CQE_PACKET_CI */
	data = readq(priv->base + MLXBF_GIGE_RX_CQE_PACKET_CI);
	dev_dbg(dev, "MLXBF_GIGE_RX_CQE_PACKET_CI=%llx\n", (u64)data);

	dev_dbg(dev, "error_intr_count=%llx\n", priv->error_intr_count);
	dev_dbg(dev, "rx_intr_count=%llx\n", priv->rx_intr_count);
	dev_dbg(dev, "llu_plu_intr_count=%llx\n", priv->llu_plu_intr_count);

	/* Display INT_EN */
	data = readq(priv->base + MLXBF_GIGE_INT_EN);
	dev_dbg(dev, "int_en=%llx\n", (u64)data);

	/* Display INT_MASK */
	data = readq(priv->base + MLXBF_GIGE_INT_MASK);
	dev_dbg(dev, "int_mask=%llx\n", (u64)data);

	return strlen(buf);
}

DEVICE_ATTR_RO(dump_rx);

/* Handler for sysfs entry 'start_tx' found at
 *   /sys/devices/platform/MLNXBF17:00/
 * Issue 'echo <N> > start_tx' to invoke this routine
 * which will send <N> dummy IP packets to GigE port
 */
static ssize_t start_tx_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	struct mlxbf_gige *priv;
	dma_addr_t tx_buf_dma;
	u8 oob_tx_data_seed;
	int oob_tx_pkt_size;
	long num_pkts = 1;
	u64 *tx_wqe_addr;
	u8 *tx_buf;
	u16 data16;
	u64 word2;
	int i, j;
	int ret;

	priv = dev_get_drvdata(dev);

	oob_tx_pkt_size = MLXBF_GIGE_DEFAULT_TX_PKT_SIZE;

	if (buf) {
		ret = kstrtol(buf, 10, &num_pkts);

		if (ret == 0) {
			dev_dbg(dev, "%s: num_pkts %d\n",
				__func__, (int)num_pkts);
		}
	}

	for (i = 0; i < num_pkts; i++) {
		/* The data seed is used to populate the packet with
		 * fake, but predictable, data.  The value of seed
		 * is stored in the first byte after the L2 header,
		 * then (seed+1) is stored in the second byte, etc.
		 */
		oob_tx_data_seed = priv->tx_data_seed;
		priv->tx_data_seed += 4;

		/* To limit output only perform debug logging if number
		 * of packets to send is less than some maximum value.
		 */
		if (num_pkts < MLXBF_GIGE_MAX_TX_PKTS_VERBOSE) {
			dev_dbg(dev, "%s: size=%x seed=%x\n",
				__func__, oob_tx_pkt_size,
				oob_tx_data_seed);
		}

		/* Allocate ptr for buffer */
		tx_buf = dma_alloc_coherent(dev, MLXBF_GIGE_DEFAULT_BUF_SZ,
					    &tx_buf_dma, GFP_KERNEL);

		if (!tx_buf)
			return -ENOMEM;

		if (num_pkts < MLXBF_GIGE_MAX_TX_PKTS_VERBOSE) {
			dev_dbg(dev, "%s: tx_buf %llx %llx\n",
				__func__, (u64)tx_buf, (u64)tx_buf_dma);
		}

		if (num_pkts < MLXBF_GIGE_MAX_TX_PKTS_VERBOSE) {
			dev_dbg(dev, "%s: pkt num %llx buffer index %x\n",
				__func__, (u64)priv->tx_pi,
				(priv->tx_pi % priv->tx_q_entries));
		}

		priv->tx_buf[priv->tx_pi % priv->tx_q_entries] = tx_buf;

		/* Put in four bytes of fake destination MAC, but use real
		 * value of 'tx_pi' in order to track TX producer index
		 * in the actual packet contents.
		 */
		*tx_buf++ = MLXBF_GIGE_FAKE_DMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_DMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_DMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_DMAC_BYTE;
		*tx_buf++ = (priv->tx_pi & 0xFF00) >> 8;
		*tx_buf++ = (priv->tx_pi & 0xFF);

		/* Put in fake source MAC */
		*tx_buf++ = MLXBF_GIGE_FAKE_SMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_SMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_SMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_SMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_SMAC_BYTE;
		*tx_buf++ = MLXBF_GIGE_FAKE_SMAC_BYTE;

		/* Set ethertype for IP (0x0800) */
		*tx_buf++ = 0x08;
		*tx_buf++ = 0x00;

		/* Put in fake packet payload */
		for (j = 0; j < (oob_tx_pkt_size - ETH_HLEN); j++)
			*tx_buf++ = (u8)(j + oob_tx_data_seed);

		/* TODO - should really reorganize all low-level TX   */
		/* logic and call it here and in 'xmit' function also */

		/* Get address of TX WQE */
		tx_wqe_addr = priv->tx_wqe_next;

		if (num_pkts < MLXBF_GIGE_MAX_TX_PKTS_VERBOSE) {
			dev_dbg(dev, "%s: tx_wqe_addr=%llx\n",
				__func__, (u64)tx_wqe_addr);
		}

		mlxbf_gige_update_tx_wqe_next(priv);

		if (num_pkts < MLXBF_GIGE_MAX_TX_PKTS_VERBOSE) {
			dev_dbg(dev, "%s: tx_wqe_next=%llx\n",
				__func__, (u64)priv->tx_wqe_next);
		}

		/* Put PA of buffer address into first 64-bit word of TX WQE */
		*tx_wqe_addr = tx_buf_dma;

		/* Set TX WQE pkt_len appropriately */
		word2 = oob_tx_pkt_size & MLXBF_GIGE_TX_WQE_PKT_LEN_MASK;

		if (num_pkts < MLXBF_GIGE_MAX_TX_PKTS_VERBOSE) {
			dev_dbg(dev, "%s: word2=%llx\n",
				__func__, (u64)word2);
		}

		/* Write entire 2nd word of TX WQE */
		*(tx_wqe_addr + 1) = word2;

		/* Create memory barrier before write to TX PI */
		wmb();

		priv->tx_pi++;

		writeq(priv->tx_pi, priv->base + MLXBF_GIGE_TX_PRODUCER_INDEX);

		if (priv->tx_pi >= 0x20) {
			data16 = readq(priv->base + MLXBF_GIGE_RX_WQE_PI) + 1;
			writeq(data16, priv->base + MLXBF_GIGE_RX_WQE_PI);
		}
	} /* end - i loop */

	return len;
}

DEVICE_ATTR_WO(start_tx);

void mlxbf_gige_create_sysfs(struct device *dev)
{
#ifdef MLXBF_GIGE_MMIO_SYSFS
	if (device_create_file(dev, &dev_attr_mmio_read))
		dev_info(dev, "failed to create mmio_read sysfs entry\n");
	if (device_create_file(dev, &dev_attr_mmio_write))
		dev_info(dev, "failed to create mmio_write sysfs entry\n");
	if (device_create_file(dev, &dev_attr_llu_mmio_read))
		dev_info(dev, "failed to create llu_mmio_read sysfs entry\n");
	if (device_create_file(dev, &dev_attr_llu_mmio_write))
		dev_info(dev, "failed to create llu_mmio_write sysfs entry\n");
	if (device_create_file(dev, &dev_attr_plu_mmio_read))
		dev_info(dev, "failed to create plu_mmio_read sysfs entry\n");
	if (device_create_file(dev, &dev_attr_plu_mmio_write))
		dev_info(dev, "failed to create plu_mmio_write sysfs entry\n");
#endif

	if (device_create_file(dev, &dev_attr_dump_rx))
		dev_info(dev, "failed to create dump_rx sysfs entry\n");
	if (device_create_file(dev, &dev_attr_dump_tx))
		dev_info(dev, "failed to create dump_tx sysfs entry\n");
	if (device_create_file(dev, &dev_attr_start_tx))
		dev_info(dev, "failed to create start_tx sysfs entry\n");
}

void mlxbf_gige_remove_sysfs(struct device *dev)
{
#ifdef MLXBF_GIGE_MMIO_SYSFS
	device_remove_file(dev, &dev_attr_mmio_read);
	device_remove_file(dev, &dev_attr_mmio_write);
	device_remove_file(dev, &dev_attr_llu_mmio_read);
	device_remove_file(dev, &dev_attr_llu_mmio_write);
	device_remove_file(dev, &dev_attr_plu_mmio_read);
	device_remove_file(dev, &dev_attr_plu_mmio_write);
#endif

	device_remove_file(dev, &dev_attr_dump_rx);
	device_remove_file(dev, &dev_attr_dump_tx);
	device_remove_file(dev, &dev_attr_start_tx);
}
#endif /* MLXBF_GIGE_INTERNAL */

static u64 mlxbf_gige_mac_to_u64(u8 *addr)
{
	u64 mac = 0;
	int i;

	for (i = 0; i < ETH_ALEN; i++) {
		mac <<= 8;
		mac |= addr[i];
	}
	return mac;
}

static void mlxbf_gige_u64_to_mac(u8 *addr, u64 mac)
{
	int i;

	for (i = ETH_ALEN; i > 0; i--) {
		addr[i - 1] = mac & 0xFF;
		mac >>= 8;
	}
}

static void mlxbf_gige_initial_mac(struct mlxbf_gige *priv)
{
	u8 mac[ETH_ALEN];
	u64 local_mac;
	int status;

	status = mlxbf_gige_get_mac_rx_filter(priv, MLXBF_GIGE_LOCAL_MAC_FILTER_IDX,
					      &local_mac);
	mlxbf_gige_u64_to_mac(mac, local_mac);

	if (is_valid_ether_addr(mac)) {
		ether_addr_copy(priv->netdev->dev_addr, mac);
#ifdef MLXBF_GIGE_INTERNAL
		dev_info(priv->dev, "Read MAC address %pM from chip\n", mac);
#endif
	} else {
		/* Provide a random MAC if for some reason the device has
		 * not been configured with a valid MAC address already.
		 */
		eth_hw_addr_random(priv->netdev);
#ifdef MLXBF_GIGE_INTERNAL
		dev_info(priv->dev, "Generated random MAC address %pM\n",
			 priv->netdev->dev_addr);
#endif
	}

	local_mac = mlxbf_gige_mac_to_u64(priv->netdev->dev_addr);
	mlxbf_gige_set_mac_rx_filter(priv, MLXBF_GIGE_LOCAL_MAC_FILTER_IDX,
				     local_mac);
}

static int mlxbf_gige_probe(struct platform_device *pdev)
{
	struct net_device *netdev;
	struct resource *mac_res;
	struct resource *llu_res;
	struct resource *plu_res;
	struct mlxbf_gige *priv;
	void __iomem *llu_base;
	void __iomem *plu_base;
	void __iomem *base;
	u64 control;
	int err = 0;

#ifdef MLXBF_GIGE_INTERNAL
	u64 exp_data;

	dev_dbg(&pdev->dev, "probe: pdev=0x%llx pdev->dev=0x%llx\n",
		(u64)pdev, (u64)&pdev->dev);
#endif

	mac_res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_MAC);
	if (!mac_res)
		return -ENXIO;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(&pdev->dev, "probe: resource %d (MAC) start=0x%llx end=0x%llx\n",
		MLXBF_GIGE_RES_MAC, mac_res->start, mac_res->end);
#endif

	base = devm_ioremap_resource(&pdev->dev, mac_res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	llu_res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_LLU);
	if (!llu_res)
		return -ENXIO;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(&pdev->dev, "probe: resource %d (LLU) start=0x%llx end=0x%llx\n",
		MLXBF_GIGE_RES_LLU, llu_res->start, llu_res->end);
#endif

	llu_base = devm_ioremap_resource(&pdev->dev, llu_res);
	if (IS_ERR(llu_base))
		return PTR_ERR(llu_base);

	plu_res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_PLU);
	if (!plu_res)
		return -ENXIO;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(&pdev->dev, "probe: resource %d (PLU) start=0x%llx end=0x%llx\n",
		MLXBF_GIGE_RES_PLU, plu_res->start, plu_res->end);
#endif

	plu_base = devm_ioremap_resource(&pdev->dev, plu_res);
	if (IS_ERR(plu_base))
		return PTR_ERR(plu_base);

#ifdef MLXBF_GIGE_INTERNAL
	/* Read single MMIO register and compare to expected value */
	exp_data = (MLXBF_GIGE_CONFIG_MAX_PKT_SZ_RESET_VAL
		    << MLXBF_GIGE_CONFIG_MAX_PKT_SZ_SHIFT);
	if (readq(base + MLXBF_GIGE_CONFIG) != exp_data) {
		dev_err(&pdev->dev,
			"probe failed, unexpected value in MLXBF_GIGE_CONFIG\n");
		return -ENODEV;
	}
#endif

	/* Perform general init of GigE block */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control |= MLXBF_GIGE_CONTROL_PORT_EN;
	writeq(control, base + MLXBF_GIGE_CONTROL);

	netdev = devm_alloc_etherdev(&pdev->dev, sizeof(*priv));
	if (!netdev) {
#ifdef MLXBF_GIGE_INTERNAL
		dev_err(&pdev->dev, "Failed to allocate etherdev\n");
#endif
		return -ENOMEM;
	}

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(&pdev->dev, "probe: netdev=%llx\n", (u64)netdev);
#endif

	SET_NETDEV_DEV(netdev, &pdev->dev);
	netdev->netdev_ops = &mlxbf_gige_netdev_ops;
	netdev->ethtool_ops = &mlxbf_gige_ethtool_ops;
	priv = netdev_priv(netdev);
	priv->netdev = netdev;

	/* Initialize feature set supported by hardware (skbuff.h) */
	netdev->hw_features = NETIF_F_RXCSUM | NETIF_F_HW_CSUM;

	platform_set_drvdata(pdev, priv);
	priv->dev = &pdev->dev;
	priv->pdev = pdev;

#ifdef FAST_MODELS
	/*
	 * TODO: Palladium has no connection to the PHY hardware, so
	 * the MDIO probe will fail.
	 * This needs to be removed once palladium provides a connection
	 * to the PHY device.
	 */

	/* Attach MDIO device */
	err = mlxbf_gige_mdio_probe(pdev, priv);
	if (err)
		return err;
#endif /* FAST_MODELS */

	priv->base = base;
	priv->llu_base = llu_base;
	priv->plu_base = plu_base;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(&pdev->dev, "probe: priv=0x%llx priv->base=0x%llx\n",
		(u64)priv, (u64)priv->base);
	dev_dbg(&pdev->dev, "probe: llu_base=0x%llx plu_base=0x%llx\n",
		(u64)priv->llu_base, (u64)priv->plu_base);

	/* Perform some self tests on MAC, PLU, LLU */
	mlxbf_gige_selftests(pdev, priv);
	mlxbf_gige_plu_selftests(pdev, priv);
	mlxbf_gige_llu_selftests(pdev, priv);
#endif

	priv->rx_q_entries = MLXBF_GIGE_DEFAULT_RXQ_SZ;
	priv->tx_q_entries = MLXBF_GIGE_DEFAULT_TXQ_SZ;

	/* Write initial MAC address to hardware */
	mlxbf_gige_initial_mac(priv);

#ifdef MLXBF_GIGE_INTERNAL
	/* Create sysfs entries for driver */
	mlxbf_gige_create_sysfs(&pdev->dev);
#endif

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(&pdev->dev, "DMA configuration failed: 0x%x\n", err);
		return err;
	}

	priv->error_irq = platform_get_irq(pdev, MLXBF_GIGE_ERROR_INTR_IDX);
	priv->rx_irq = platform_get_irq(pdev, MLXBF_GIGE_RECEIVE_PKT_INTR_IDX);
	priv->llu_plu_irq = platform_get_irq(pdev, MLXBF_GIGE_LLU_PLU_INTR_IDX);

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(&pdev->dev, "probe: irq[] = %d %d %d\n",
		priv->error_irq, priv->rx_irq, priv->llu_plu_irq);
#endif

	err = register_netdev(netdev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register netdev\n");
		return err;
	}

#ifdef MLXBF_GIGE_INTERNAL
	dev_info(&pdev->dev, "probed\n");

	priv->tx_data_seed = 0xB0;
#endif

	return 0;
}

/* Device remove function. */
static int mlxbf_gige_remove(struct platform_device *pdev)
{
	struct mlxbf_gige *priv;

#ifdef MLXBF_GIGE_INTERNAL
	dev_dbg(&pdev->dev, "remove: pdev=%llx\n", (u64)pdev);
#endif

	priv = platform_get_drvdata(pdev);

	unregister_netdev(priv->netdev);

#ifdef MLXBF_GIGE_INTERNAL
	/* Remove driver sysfs entries */
	mlxbf_gige_remove_sysfs(&pdev->dev);
#endif

#ifdef FAST_MODELS
	/* Remove mdio */
	mlxbf_gige_mdio_remove(priv);
#endif /* FAST_MODELS */

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct acpi_device_id mlxbf_gige_acpi_match[] = {
	{ "MLNXBF17", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, mlxbf_gige_acpi_match);

static struct platform_driver mlxbf_gige_driver = {
	.probe = mlxbf_gige_probe,
	.remove = mlxbf_gige_remove,
	.driver = {
		.name = DRV_NAME,
		.acpi_match_table = ACPI_PTR(mlxbf_gige_acpi_match),
	},
};

module_platform_driver(mlxbf_gige_driver);

MODULE_DESCRIPTION("Mellanox BlueField SoC Gigabit Ethernet Driver");
MODULE_AUTHOR("David Thompson <dthompson@mellanox.com>");
MODULE_AUTHOR("Asmaa Mnebhi <asmaa@mellanox.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
