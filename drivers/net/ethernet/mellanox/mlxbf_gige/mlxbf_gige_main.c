// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause

/* Gigabit Ethernet driver for Mellanox BlueField SoC
 *
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 */

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>

#include "mlxbf_gige.h"
#include "mlxbf_gige_regs.h"

#define DRV_NAME    "mlxbf_gige"
#define DRV_VERSION "1.3"

static void mlxbf_gige_set_mac_rx_filter(struct mlxbf_gige *priv,
					 unsigned int index, u64 dmac)
{
	void __iomem *base = priv->base;
	u64 control;

	/* Write destination MAC to specified MAC RX filter */
	writeq(dmac, base + MLXBF_GIGE_RX_MAC_FILTER +
	       (index * MLXBF_GIGE_RX_MAC_FILTER_STRIDE));

	/* Enable MAC receive filter mask for specified index */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control |= (MLXBF_GIGE_CONTROL_EN_SPECIFIC_MAC << index);
	writeq(control, base + MLXBF_GIGE_CONTROL);
}

static void mlxbf_gige_get_mac_rx_filter(struct mlxbf_gige *priv,
					 unsigned int index, u64 *dmac)
{
	void __iomem *base = priv->base;

	/* Read destination MAC from specified MAC RX filter */
	*dmac = readq(base + MLXBF_GIGE_RX_MAC_FILTER +
		      (index * MLXBF_GIGE_RX_MAC_FILTER_STRIDE));
}

static void mlxbf_gige_enable_promisc(struct mlxbf_gige *priv)
{
	void __iomem *base = priv->base;
	u64 control;

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

	/* Initialize 'rx_wqe_ptr' to point to first RX WQE in array
	 * Each RX WQE is simply a receive buffer pointer, so walk
	 * the entire array, allocating a 2KB buffer for each element
	 */
	rx_wqe_ptr = priv->rx_wqe_base;

	for (i = 0; i < priv->rx_q_entries; i++) {
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

	priv->rx_wqe_base = NULL;
	priv->rx_wqe_base_dma = 0;
	priv->rx_cqe_base = NULL;
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

	priv->tx_wqe_base = NULL;
	priv->tx_wqe_base_dma = 0;
	priv->tx_cc = NULL;
	priv->tx_cc_dma = 0;
	priv->tx_wqe_next = NULL;
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
	__be64 *buff = p;
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
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

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

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void mlxbf_gige_get_pauseparam(struct net_device *netdev,
				      struct ethtool_pauseparam *pause)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);

	pause->autoneg = priv->aneg_pause;
	pause->rx_pause = priv->tx_pause;
	pause->tx_pause = priv->rx_pause;
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
	.nway_reset		= phy_ethtool_nway_reset,
	.get_pauseparam		= mlxbf_gige_get_pauseparam,
	.get_link_ksettings	= phy_ethtool_get_link_ksettings,
};

/* Start of struct net_device_ops functions */
static irqreturn_t mlxbf_gige_error_intr(int irq, void *dev_id)
{
	struct mlxbf_gige *priv;
	u64 int_status;

	priv = dev_id;

	priv->error_intr_count++;

	int_status = readq(priv->base + MLXBF_GIGE_INT_STATUS);

	if (int_status & MLXBF_GIGE_INT_STATUS_HW_ACCESS_ERROR)
		priv->stats.hw_access_errors++;

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_CHECKSUM_INPUTS) {
		priv->stats.tx_invalid_checksums++;
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

	if (int_status & MLXBF_GIGE_INT_STATUS_TX_PI_CI_EXCEED_WQ_SIZE)
		priv->stats.tx_index_errors++;

	if (int_status & MLXBF_GIGE_INT_STATUS_SW_CONFIG_ERROR)
		priv->stats.sw_config_errors++;

	if (int_status & MLXBF_GIGE_INT_STATUS_SW_ACCESS_ERROR)
		priv->stats.sw_access_errors++;

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
	unsigned long flags;
	u16 avail;

	spin_lock_irqsave(&priv->lock, flags);

	if (priv->prev_tx_ci == priv->tx_pi)
		avail = priv->tx_q_entries - 1;
	else
		avail = ((priv->tx_q_entries + priv->prev_tx_ci - priv->tx_pi)
			  % priv->tx_q_entries) - 1;

	spin_unlock_irqrestore(&priv->lock, flags);

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
		tx_wqe_addr = priv->tx_wqe_base +
			       (tx_wqe_index * MLXBF_GIGE_TX_WQE_SZ_QWORDS);

		stats->tx_packets++;
		stats->tx_bytes += MLXBF_GIGE_TX_WQE_PKT_LEN(tx_wqe_addr);
		dma_free_coherent(priv->dev, MLXBF_GIGE_DEFAULT_BUF_SZ,
				  priv->tx_buf[tx_wqe_index], *tx_wqe_addr);
		priv->tx_buf[tx_wqe_index] = NULL;
	}

	/* Since the TX ring was likely just drained, check if TX queue
	 * had previously been stopped and now that there are TX buffers
	 * available the TX queue can be awakened.
	 */
	if (netif_queue_stopped(priv->netdev) &&
	    mlxbf_gige_tx_buffs_avail(priv)) {
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

		skb = dev_alloc_skb(datalen);
		if (!skb) {
			netdev->stats.rx_dropped++;
			return false;
		}

		memcpy(skb_put(skb, datalen), pktp, datalen);

		skb->dev = netdev;
		skb->protocol = eth_type_trans(skb, netdev);
		skb->ip_summed = CHECKSUM_NONE; /* device did not checksum packet */

		netif_receive_skb(skb);
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MAC_ERR) {
		priv->stats.rx_mac_errors++;
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_TRUNCATED) {
		priv->stats.rx_truncate_errors++;
	}

	/* Let hardware know we've replenished one buffer */
	writeq(rx_pi + 1, priv->base + MLXBF_GIGE_RX_WQE_PI);

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

	err = request_threaded_irq(priv->phy_irq, NULL,
			       mlxbf_gige_mdio_handle_phy_interrupt,
			       IRQF_ONESHOT | IRQF_SHARED, "mlxbf_gige_phy",
			       priv);
	if (err) {
		dev_err(priv->dev, "Request phy_irq failure\n");
		return err;
	}

	return 0;
}

static void mlxbf_gige_free_irqs(struct mlxbf_gige *priv)
{
	devm_free_irq(priv->dev, priv->error_irq, priv);
	devm_free_irq(priv->dev, priv->rx_irq, priv);
	devm_free_irq(priv->dev, priv->llu_plu_irq, priv);
	free_irq(priv->phy_irq, priv);
}

static void mlxbf_gige_cache_stats(struct mlxbf_gige *priv)
{
	struct mlxbf_gige_stats *p;

	/* Cache stats that will be cleared by clean port operation */
	p = &priv->stats;
	p->rx_din_dropped_pkts += readq(priv->base +
					MLXBF_GIGE_RX_DIN_DROP_COUNTER);
	p->rx_filter_passed_pkts += readq(priv->base +
					  MLXBF_GIGE_RX_PASS_COUNTER_ALL);
	p->rx_filter_discard_pkts += readq(priv->base +
					   MLXBF_GIGE_RX_DISC_COUNTER_ALL);
}

static void mlxbf_gige_clean_port(struct mlxbf_gige *priv)
{
	u64 control, status;
	int cnt;

	/* Set the CLEAN_PORT_EN bit to trigger SW reset */
	control = readq(priv->base + MLXBF_GIGE_CONTROL);
	control |= MLXBF_GIGE_CONTROL_CLEAN_PORT_EN;
	writeq(control, priv->base + MLXBF_GIGE_CONTROL);

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

static int mlxbf_gige_phy_enable_interrupt(struct phy_device *phydev)
{
	int ret = 0;

	if (phydev->drv->ack_interrupt)
		ret = phydev->drv->ack_interrupt(phydev);
	if (ret < 0)
		return ret;

	phydev->interrupts = PHY_INTERRUPT_ENABLED;
	if (phydev->drv->config_intr)
		ret = phydev->drv->config_intr(phydev);

	return ret;
}

static int mlxbf_gige_phy_disable_interrupt(struct phy_device *phydev)
{
	int ret = 0;

	if (phydev->drv->ack_interrupt)
		ret = phydev->drv->ack_interrupt(phydev);
	if (ret < 0)
		return ret;

	phydev->interrupts = PHY_INTERRUPT_DISABLED;
	if (phydev->drv->config_intr)
		ret = phydev->drv->config_intr(phydev);

	return ret;
}

static int mlxbf_gige_open(struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);
	struct phy_device *phydev = netdev->phydev;
	u64 int_en;
	int err;

	mlxbf_gige_cache_stats(priv);
	mlxbf_gige_clean_port(priv);
	mlxbf_gige_rx_init(priv);
	mlxbf_gige_tx_init(priv);
	netif_napi_add(netdev, &priv->napi, mlxbf_gige_poll, NAPI_POLL_WEIGHT);
	napi_enable(&priv->napi);
	netif_start_queue(netdev);

	err = mlxbf_gige_request_irqs(priv);
	if (err)
		return err;

	phy_start(phydev);
	/* Always make sure interrupts are enabled since phy_start calls
	 * __phy_resume which may reset the PHY interrupt control reg.
	 * __phy_resume only reenables the interrupts if
	 * phydev->irq != IRQ_IGNORE_INTERRUPT.
	 */
	err = mlxbf_gige_phy_enable_interrupt(phydev);
	if (err)
		return err;

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

static int mlxbf_gige_stop(struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);

	writeq(0, priv->base + MLXBF_GIGE_INT_EN);
	netif_stop_queue(netdev);
	napi_disable(&priv->napi);
	netif_napi_del(&priv->napi);
	mlxbf_gige_free_irqs(priv);

	phy_stop(netdev->phydev);
	mlxbf_gige_phy_disable_interrupt(netdev->phydev);

	mlxbf_gige_rx_deinit(priv);
	mlxbf_gige_tx_deinit(priv);
	mlxbf_gige_cache_stats(priv);
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
	struct mlxbf_gige *priv = netdev_priv(netdev);
	dma_addr_t tx_buf_dma;
	u8 *tx_buf = NULL;
	u64 *tx_wqe_addr;
	u64 word2;

	/* Check that there is room left in TX ring */
	if (!mlxbf_gige_tx_buffs_avail(priv)) {
		/* TX ring is full, inform stack but do not free SKB */
		netif_stop_queue(netdev);
		netdev->stats.tx_dropped++;
		/* Since there is no separate "TX complete" interrupt, need
		 * to explicitly schedule NAPI poll.  This will trigger logic
		 * which processes TX completions, and will hopefully drain
		 * the TX ring allowing the TX queue to be awakened.
		 */
		napi_schedule(&priv->napi);
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

	/* Free incoming skb, contents already copied to HW */
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static int mlxbf_gige_do_ioctl(struct net_device *netdev,
			       struct ifreq *ifr, int cmd)
{
	if (!(netif_running(netdev)))
		return -EINVAL;

	return phy_mii_ioctl(netdev->phydev, ifr, cmd);
}

static void mlxbf_gige_set_rx_mode(struct net_device *netdev)
{
	struct mlxbf_gige *priv = netdev_priv(netdev);
	bool new_promisc_enabled;

	new_promisc_enabled = netdev->flags & IFF_PROMISC;

	/* Only write to the hardware registers if the new setting
	 * of promiscuous mode is different from the current one.
	 */
	if (new_promisc_enabled != priv->promisc_enabled) {
		priv->promisc_enabled = new_promisc_enabled;

		if (new_promisc_enabled)
			mlxbf_gige_enable_promisc(priv);
		else
			mlxbf_gige_disable_promisc(priv);
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
};

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

	mlxbf_gige_get_mac_rx_filter(priv, MLXBF_GIGE_LOCAL_MAC_FILTER_IDX,
				     &local_mac);
	mlxbf_gige_u64_to_mac(mac, local_mac);

	if (is_valid_ether_addr(mac)) {
		ether_addr_copy(priv->netdev->dev_addr, mac);
	} else {
		/* Provide a random MAC if for some reason the device has
		 * not been configured with a valid MAC address already.
		 */
		eth_hw_addr_random(priv->netdev);
	}

	local_mac = mlxbf_gige_mac_to_u64(priv->netdev->dev_addr);
	mlxbf_gige_set_mac_rx_filter(priv, MLXBF_GIGE_LOCAL_MAC_FILTER_IDX,
				     local_mac);
}

static void mlxbf_gige_adjust_link(struct net_device *netdev)
{
	/* Only one speed and one duplex supported */
	return;
}

static int mlxbf_gige_probe(struct platform_device *pdev)
{
	struct phy_device *phydev;
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
	int addr;

	mac_res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_MAC);
	if (!mac_res)
		return -ENXIO;

	base = devm_ioremap_resource(&pdev->dev, mac_res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	llu_res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_LLU);
	if (!llu_res)
		return -ENXIO;

	llu_base = devm_ioremap_resource(&pdev->dev, llu_res);
	if (IS_ERR(llu_base))
		return PTR_ERR(llu_base);

	plu_res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_PLU);
	if (!plu_res)
		return -ENXIO;

	plu_base = devm_ioremap_resource(&pdev->dev, plu_res);
	if (IS_ERR(plu_base))
		return PTR_ERR(plu_base);

	/* Perform general init of GigE block */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control |= MLXBF_GIGE_CONTROL_PORT_EN;
	writeq(control, base + MLXBF_GIGE_CONTROL);

	netdev = devm_alloc_etherdev(&pdev->dev, sizeof(*priv));
	if (!netdev)
		return -ENOMEM;

	SET_NETDEV_DEV(netdev, &pdev->dev);
	netdev->netdev_ops = &mlxbf_gige_netdev_ops;
	netdev->ethtool_ops = &mlxbf_gige_ethtool_ops;
	priv = netdev_priv(netdev);
	priv->netdev = netdev;

	platform_set_drvdata(pdev, priv);
	priv->dev = &pdev->dev;
	priv->pdev = pdev;

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->gpio_lock);

	/* Attach MDIO device */
	err = mlxbf_gige_mdio_probe(pdev, priv);
	if (err)
		return err;

	priv->base = base;
	priv->llu_base = llu_base;
	priv->plu_base = plu_base;

	priv->rx_q_entries = MLXBF_GIGE_DEFAULT_RXQ_SZ;
	priv->tx_q_entries = MLXBF_GIGE_DEFAULT_TXQ_SZ;

	/* Write initial MAC address to hardware */
	mlxbf_gige_initial_mac(priv);

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(&pdev->dev, "DMA configuration failed: 0x%x\n", err);
		return err;
	}

	priv->error_irq = platform_get_irq(pdev, MLXBF_GIGE_ERROR_INTR_IDX);
	priv->rx_irq = platform_get_irq(pdev, MLXBF_GIGE_RECEIVE_PKT_INTR_IDX);
	priv->llu_plu_irq = platform_get_irq(pdev, MLXBF_GIGE_LLU_PLU_INTR_IDX);
	priv->phy_irq = platform_get_irq(pdev, MLXBF_GIGE_PHY_INT_N);

	phydev = phy_find_first(priv->mdiobus);
	if (!phydev)
		return -ENODEV;

	addr = phydev->mdio.addr;
	phydev->irq = priv->mdiobus->irq[addr] = PHY_IGNORE_INTERRUPT;

	/* Sets netdev->phydev to phydev; which will eventually
	 * be used in ioctl calls.
	 * Cannot pass NULL handler.
	 */
	err = phy_connect_direct(netdev, phydev,
				 mlxbf_gige_adjust_link,
				 PHY_INTERFACE_MODE_GMII);
	if (err) {
		dev_err(&pdev->dev, "Could not attach to PHY\n");
		return err;
	}

	/* MAC only supports 1000T full duplex mode */
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_1000baseT_Half_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_100baseT_Full_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_100baseT_Half_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_10baseT_Full_BIT);
	phy_remove_link_mode(phydev, ETHTOOL_LINK_MODE_10baseT_Half_BIT);

	/* MAC supports symmetric flow control */
	phy_support_sym_pause(phydev);

	/* Enable pause */
	priv->rx_pause = phydev->pause;
	priv->tx_pause = phydev->pause;
	priv->aneg_pause = AUTONEG_ENABLE;

	/* Display information about attached PHY device */
	phy_attached_info(phydev);

	err = register_netdev(netdev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register netdev\n");
		phy_disconnect(phydev);
		return err;
	}

	return 0;
}

static int mlxbf_gige_remove(struct platform_device *pdev)
{
	struct mlxbf_gige *priv = platform_get_drvdata(pdev);

	unregister_netdev(priv->netdev);
	phy_disconnect(priv->netdev->phydev);
	mlxbf_gige_mdio_remove(priv);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void mlxbf_gige_shutdown(struct platform_device *pdev)
{
	struct mlxbf_gige *priv = platform_get_drvdata(pdev);

	writeq(0, priv->base + MLXBF_GIGE_INT_EN);
	mlxbf_gige_clean_port(priv);
}

static const struct acpi_device_id mlxbf_gige_acpi_match[] = {
	{ "MLNXBF17", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, mlxbf_gige_acpi_match);

static struct platform_driver mlxbf_gige_driver = {
	.probe = mlxbf_gige_probe,
	.remove = mlxbf_gige_remove,
	.shutdown = mlxbf_gige_shutdown,
	.driver = {
		.name = DRV_NAME,
		.acpi_match_table = ACPI_PTR(mlxbf_gige_acpi_match),
	},
};

module_platform_driver(mlxbf_gige_driver);

MODULE_DESCRIPTION("Mellanox BlueField SoC Gigabit Ethernet Driver");
MODULE_AUTHOR("David Thompson <dthompson@mellanox.com>");
MODULE_AUTHOR("Asmaa Mnebhi <asmaa@mellanox.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);
