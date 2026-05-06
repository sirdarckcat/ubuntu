// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause

/* Packet receive logic for Mellanox Gigabit Ethernet driver
 *
 * Copyright (C) 2020-2021 NVIDIA CORPORATION & AFFILIATES
 */

#include <linux/etherdevice.h>
#include <linux/if.h>
#include <linux/skbuff.h>

#include "mlxbf_gige.h"
#include "mlxbf_gige_regs.h"

void mlxbf_gige_enable_multicast_rx(struct mlxbf_gige *priv)
{
	void __iomem *base = priv->base;
	u64 data;

	data = readq(base + MLXBF_GIGE_RX_MAC_FILTER_GENERAL);
	data |= MLXBF_GIGE_RX_MAC_FILTER_EN_MULTICAST;
	writeq(data, base + MLXBF_GIGE_RX_MAC_FILTER_GENERAL);
}

void mlxbf_gige_disable_multicast_rx(struct mlxbf_gige *priv)
{
	void __iomem *base = priv->base;
	u64 data;

	data = readq(base + MLXBF_GIGE_RX_MAC_FILTER_GENERAL);
	data &= ~MLXBF_GIGE_RX_MAC_FILTER_EN_MULTICAST;
	writeq(data, base + MLXBF_GIGE_RX_MAC_FILTER_GENERAL);
}

void mlxbf_gige_enable_mac_rx_filter(struct mlxbf_gige *priv,
				     unsigned int index)
{
	void __iomem *base = priv->base;
	u64 control;

	/* Enable MAC receive filter mask for specified index */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control |= (MLXBF_GIGE_CONTROL_EN_SPECIFIC_MAC << index);
	writeq(control, base + MLXBF_GIGE_CONTROL);
}

void mlxbf_gige_disable_mac_rx_filter(struct mlxbf_gige *priv,
				      unsigned int index)
{
	void __iomem *base = priv->base;
	u64 control;

	/* Disable MAC receive filter mask for specified index */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control &= ~(MLXBF_GIGE_CONTROL_EN_SPECIFIC_MAC << index);
	writeq(control, base + MLXBF_GIGE_CONTROL);
}

void mlxbf_gige_set_mac_rx_filter(struct mlxbf_gige *priv,
				  unsigned int index, u64 dmac)
{
	void __iomem *base = priv->base;

	/* Write destination MAC to specified MAC RX filter */
	writeq(dmac, base + MLXBF_GIGE_RX_MAC_FILTER +
	       (index * MLXBF_GIGE_RX_MAC_FILTER_STRIDE));
}

void mlxbf_gige_get_mac_rx_filter(struct mlxbf_gige *priv,
				  unsigned int index, u64 *dmac)
{
	void __iomem *base = priv->base;

	/* Read destination MAC from specified MAC RX filter */
	*dmac = readq(base + MLXBF_GIGE_RX_MAC_FILTER +
		      (index * MLXBF_GIGE_RX_MAC_FILTER_STRIDE));
}

void mlxbf_gige_enable_mac_rx_range(struct mlxbf_gige *priv,
				    u64 start_mac, u64 end_mac)
{
	void __iomem *base = priv->base;
	u64 control;

	writeq(start_mac, base + MLXBF_GIGE_RX_MAC_FILTER_DMAC_RANGE_START);
	writeq(end_mac, base + MLXBF_GIGE_RX_MAC_FILTER_DMAC_RANGE_END);

	/* Enable MAC_ID_RANGE match functionality */
	control = readq(base + MLXBF_GIGE_CONTROL);
	control |= MLXBF_GIGE_CONTROL_MAC_ID_RANGE_EN;
	writeq(control, base + MLXBF_GIGE_CONTROL);
}

void mlxbf_gige_disable_mac_rx_range(struct mlxbf_gige *priv)
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

void mlxbf_gige_enable_promisc(struct mlxbf_gige *priv)
{
	mlxbf_gige_enable_mac_rx_range(priv, 0, BCAST_MAC_ADDR);
}

void mlxbf_gige_disable_promisc(struct mlxbf_gige *priv)
{
	mlxbf_gige_disable_mac_rx_range(priv);
}

/* Configure data & mask filter to match upper 24-bits of multicast MAC */
static void mlxbf_gige_write_rx_mcast(struct mlxbf_gige *priv, u32 data, u32 mask)
{
	u64 v;

	v = ((u64)(data & 0xffffff) << MLXBF_GIGE_RX_MAC_FILTER_MULTICAST_DATA_SHIFT) |
	    (mask & 0xffffff);
	writeq(v, priv->base + MLXBF_GIGE_RX_MAC_FILTER_MULTICAST);
}

void mlxbf_gige_set_rx_mode_multicast(struct mlxbf_gige *priv,
				      struct net_device *netdev)
{
	const u8 *addr0 = NULL, *addr1 = NULL, *addr2 = NULL;
	struct netdev_hw_addr *ha;
	unsigned int mc_count = 0;
	u64 addr2_u64;

	/* Promiscuous mode: no need to configure multicast filters */
	if (netdev->flags & IFF_PROMISC)
		return;

	netdev_for_each_mc_addr(ha, netdev) {
		if (mc_count == 0)
			addr0 = ha->addr;
		else if (mc_count == 1)
			addr1 = ha->addr;
		else if (mc_count == 2)
			addr2 = ha->addr;
		mc_count++;
	}

	/* ALLMULTI mode or 4+ multicast MACs: allow all packets with multicast bit set */
	if ((netdev->flags & IFF_MULTICAST) &&
	    ((netdev->flags & IFF_ALLMULTI) || mc_count > 3)) {
		mlxbf_gige_write_rx_mcast(priv, BIT_ULL(40) >> 24, BIT_ULL(40) >> 24);
		mlxbf_gige_enable_multicast_rx(priv);
		return;
	}

	/* No multicast mode: turn off all multicast filter mechanisms */
	if (((netdev->flags & IFF_MULTICAST) == 0) ||
	    mc_count == 0) {
		mlxbf_gige_disable_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX0);
		mlxbf_gige_disable_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX1);
		mlxbf_gige_disable_mac_rx_range(priv);
		mlxbf_gige_disable_multicast_rx(priv);
		return;
	}

	/* Disable ALLMULTI mode for 1-3 multicast MACs */
	mlxbf_gige_disable_multicast_rx(priv);

	/* Configure up to 2 multicast MACs in direct-match MAC filter slots 2 and 3 */
	if (addr0) {
		mlxbf_gige_set_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX0,
					     ether_addr_to_u64(addr0));
		mlxbf_gige_enable_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX0);
	} else {
		mlxbf_gige_disable_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX0);
	}
	if (mc_count > 1 && addr1) {
		mlxbf_gige_set_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX1,
					     ether_addr_to_u64(addr1));
		mlxbf_gige_enable_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX1);
	} else {
		mlxbf_gige_disable_mac_rx_filter(priv, MLXBF_GIGE_MCAST_MAC_FILTER_IDX1);
	}
	/* Configure 3rd multicast MAC (if any) into 48-bit range compare */
	if (mc_count > 2 && addr2) {
		addr2_u64 = ether_addr_to_u64(addr2);
		mlxbf_gige_enable_mac_rx_range(priv, addr2_u64, addr2_u64);
	} else {
		mlxbf_gige_disable_mac_rx_range(priv);
	}
}

/* Receive Initialization
 * 1) Configures RX MAC filters via MMIO registers
 * 2) Allocates RX WQE array using coherent DMA mapping
 * 3) Initializes each element of RX WQE array with a receive
 *    buffer pointer (also using coherent DMA mapping)
 * 4) Allocates RX CQE array using coherent DMA mapping
 * 5) Completes other misc receive initialization
 */
int mlxbf_gige_rx_init(struct mlxbf_gige *priv)
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
		priv->rx_skb[i] = mlxbf_gige_alloc_skb(priv, MLXBF_GIGE_DEFAULT_BUF_SZ,
						       &rx_buf_dma, DMA_FROM_DEVICE);
		if (!priv->rx_skb[i])
			goto free_wqe_and_skb;
		*rx_wqe_ptr++ = rx_buf_dma;
	}

	/* Write RX WQE base address into MMIO reg */
	writeq(priv->rx_wqe_base_dma, priv->base + MLXBF_GIGE_RX_WQ_BASE);

	cq_size = MLXBF_GIGE_RX_CQE_SZ * priv->rx_q_entries;
	priv->rx_cqe_base = dma_alloc_coherent(priv->dev, cq_size,
					       &priv->rx_cqe_base_dma,
					       GFP_KERNEL);
	if (!priv->rx_cqe_base)
		goto free_wqe_and_skb;

	for (i = 0; i < priv->rx_q_entries; i++)
		priv->rx_cqe_base[i] |= MLXBF_GIGE_RX_CQE_VALID_MASK;

	/* Write RX CQE base address into MMIO reg */
	writeq(priv->rx_cqe_base_dma, priv->base + MLXBF_GIGE_RX_CQ_BASE);

	/* Write RX_WQE_PI with current number of replenished buffers */
	writeq(priv->rx_q_entries, priv->base + MLXBF_GIGE_RX_WQE_PI);

	/* Enable removal of CRC during RX */
	data = readq(priv->base + MLXBF_GIGE_RX);
	data |= MLXBF_GIGE_RX_STRIP_CRC_EN;
	writeq(data, priv->base + MLXBF_GIGE_RX);

	/* Enable RX MAC filter pass and discard counters */
	writeq(MLXBF_GIGE_RX_MAC_FILTER_COUNT_DISC_EN,
	       priv->base + MLXBF_GIGE_RX_MAC_FILTER_COUNT_DISC);
	writeq(MLXBF_GIGE_RX_MAC_FILTER_COUNT_PASS_EN,
	       priv->base + MLXBF_GIGE_RX_MAC_FILTER_COUNT_PASS);

	writeq(ilog2(priv->rx_q_entries),
	       priv->base + MLXBF_GIGE_RX_WQE_SIZE_LOG2);

	/* Clear MLXBF_GIGE_INT_MASK 'receive pkt' bit to
	 * indicate readiness to receive interrupts
	 */
	data = readq(priv->base + MLXBF_GIGE_INT_MASK);
	data &= ~MLXBF_GIGE_INT_MASK_RX_RECEIVE_PACKET;
	writeq(data, priv->base + MLXBF_GIGE_INT_MASK);

	/* Enable RX DMA to write new packets to memory */
	data = readq(priv->base + MLXBF_GIGE_RX_DMA);
	data |= MLXBF_GIGE_RX_DMA_EN;
	writeq(data, priv->base + MLXBF_GIGE_RX_DMA);

	return 0;

free_wqe_and_skb:
	rx_wqe_ptr = priv->rx_wqe_base;
	for (j = 0; j < i; j++) {
		dma_unmap_single(priv->dev, *rx_wqe_ptr,
				 MLXBF_GIGE_DEFAULT_BUF_SZ, DMA_FROM_DEVICE);
		dev_kfree_skb(priv->rx_skb[j]);
		rx_wqe_ptr++;
	}
	dma_free_coherent(priv->dev, wq_size,
			  priv->rx_wqe_base, priv->rx_wqe_base_dma);
	return -ENOMEM;
}

/* Receive Deinitialization
 * This routine will free allocations done by mlxbf_gige_rx_init(),
 * namely the RX WQE and RX CQE arrays, as well as all RX buffers
 */
void mlxbf_gige_rx_deinit(struct mlxbf_gige *priv)
{
	dma_addr_t *rx_wqe_ptr;
	size_t size;
	u64 data;
	int i;

	/* Disable RX DMA to prevent packet transfers to memory */
	data = readq(priv->base + MLXBF_GIGE_RX_DMA);
	data &= ~MLXBF_GIGE_RX_DMA_EN;
	writeq(data, priv->base + MLXBF_GIGE_RX_DMA);

	rx_wqe_ptr = priv->rx_wqe_base;

	for (i = 0; i < priv->rx_q_entries; i++) {
		dma_unmap_single(priv->dev, *rx_wqe_ptr, MLXBF_GIGE_DEFAULT_BUF_SZ,
				 DMA_FROM_DEVICE);
		dev_kfree_skb(priv->rx_skb[i]);
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

static bool mlxbf_gige_rx_packet(struct mlxbf_gige *priv, int *rx_pkts)
{
	struct net_device *netdev = priv->netdev;
	struct sk_buff *skb = NULL, *rx_skb;
	u16 rx_pi_rem, rx_ci_rem;
	dma_addr_t *rx_wqe_addr;
	dma_addr_t rx_buf_dma;
	u64 *rx_cqe_addr;
	u64 datalen;
	u64 rx_cqe;
	u16 rx_ci;
	u16 rx_pi;

	/* Index into RX buffer array is rx_pi w/wrap based on RX_CQE_SIZE */
	rx_pi = readq(priv->base + MLXBF_GIGE_RX_WQE_PI);
	rx_pi_rem = rx_pi % priv->rx_q_entries;

	rx_wqe_addr = priv->rx_wqe_base + rx_pi_rem;
	rx_cqe_addr = priv->rx_cqe_base + rx_pi_rem;
	rx_cqe = *rx_cqe_addr;

	if ((!!(rx_cqe & MLXBF_GIGE_RX_CQE_VALID_MASK)) != priv->valid_polarity)
		return false;

	if ((rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MASK) == 0) {
		/* Packet is OK, increment stats */
		datalen = rx_cqe & MLXBF_GIGE_RX_CQE_PKT_LEN_MASK;
		netdev->stats.rx_packets++;
		netdev->stats.rx_bytes += datalen;

		skb = priv->rx_skb[rx_pi_rem];

		/* Alloc another RX SKB for this same index */
		rx_skb = mlxbf_gige_alloc_skb(priv, MLXBF_GIGE_DEFAULT_BUF_SZ,
					      &rx_buf_dma, DMA_FROM_DEVICE);
		if (!rx_skb)
			return false;
		priv->rx_skb[rx_pi_rem] = rx_skb;
		dma_unmap_single(priv->dev, *rx_wqe_addr,
				 MLXBF_GIGE_DEFAULT_BUF_SZ, DMA_FROM_DEVICE);

		skb_put(skb, datalen);

		skb->ip_summed = CHECKSUM_NONE; /* device did not checksum packet */

		skb->protocol = eth_type_trans(skb, netdev);

		*rx_wqe_addr = rx_buf_dma;
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MAC_ERR) {
		priv->stats.rx_mac_errors++;
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_TRUNCATED) {
		priv->stats.rx_truncate_errors++;
	}

	/* Read receive consumer index before replenish so that this routine
	 * returns accurate return value even if packet is received into
	 * just-replenished buffer prior to exiting this routine.
	 */
	rx_ci = readq(priv->base + MLXBF_GIGE_RX_CQE_PACKET_CI);
	rx_ci_rem = rx_ci % priv->rx_q_entries;

	/* Let hardware know we've replenished one buffer */
	rx_pi++;

	/* Ensure completion of all writes before notifying HW of replenish */
	wmb();
	writeq(rx_pi, priv->base + MLXBF_GIGE_RX_WQE_PI);

	(*rx_pkts)++;

	rx_pi_rem = rx_pi % priv->rx_q_entries;
	if (rx_pi_rem == 0)
		priv->valid_polarity ^= 1;

	if (skb)
		netif_receive_skb(skb);

	return rx_pi_rem != rx_ci_rem;
}

/* Driver poll() function called by NAPI infrastructure */
int mlxbf_gige_poll(struct napi_struct *napi, int budget)
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
