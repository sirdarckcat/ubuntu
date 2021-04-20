// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause

/* Packet receive logic for Mellanox Gigabit Ethernet driver
 *
 * Copyright (c) 2020-2021 NVIDIA Corporation.
 */

#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "mlxbf_gige.h"
#include "mlxbf_gige_regs.h"

void mlxbf_gige_set_mac_rx_filter(struct mlxbf_gige *priv,
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

void mlxbf_gige_get_mac_rx_filter(struct mlxbf_gige *priv,
				  unsigned int index, u64 *dmac)
{
	void __iomem *base = priv->base;

	/* Read destination MAC from specified MAC RX filter */
	*dmac = readq(base + MLXBF_GIGE_RX_MAC_FILTER +
		      (index * MLXBF_GIGE_RX_MAC_FILTER_STRIDE));
}

void mlxbf_gige_enable_promisc(struct mlxbf_gige *priv)
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

void mlxbf_gige_disable_promisc(struct mlxbf_gige *priv)
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
		priv->rx_skb[i] = mlxbf_gige_alloc_skb(priv, &rx_buf_dma, DMA_FROM_DEVICE);
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

	/* Clear MLXBF_GIGE_INT_MASK 'receive pkt' bit to
	 * indicate readiness to receive interrupts
	 */
	data = readq(priv->base + MLXBF_GIGE_INT_MASK);
	data &= ~MLXBF_GIGE_INT_MASK_RX_RECEIVE_PACKET;
	writeq(data, priv->base + MLXBF_GIGE_INT_MASK);

	/* Enable RX DMA to write new packets to memory */
	writeq(MLXBF_GIGE_RX_DMA_EN, priv->base + MLXBF_GIGE_RX_DMA);

	writeq(ilog2(priv->rx_q_entries),
	       priv->base + MLXBF_GIGE_RX_WQE_SIZE_LOG2);

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
	int i;

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
	u16 rx_pi_rem, rx_ci_rem;
	dma_addr_t *rx_wqe_addr;
	dma_addr_t rx_buf_dma;
	struct sk_buff *skb;
	u64 *rx_cqe_addr;
	u64 datalen;
	u64 rx_cqe;
	u16 rx_ci;
	u16 rx_pi;

	/* Index into RX buffer array is rx_pi w/wrap based on RX_CQE_SIZE */
	rx_pi = readq(priv->base + MLXBF_GIGE_RX_WQE_PI);
	rx_pi_rem = rx_pi % priv->rx_q_entries;
	rx_wqe_addr = priv->rx_wqe_base + rx_pi_rem;

	dma_unmap_single(priv->dev, *rx_wqe_addr,
			 MLXBF_GIGE_DEFAULT_BUF_SZ, DMA_FROM_DEVICE);

	rx_cqe_addr = priv->rx_cqe_base + rx_pi_rem;
	rx_cqe = *rx_cqe_addr;

	if ((rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MASK) == 0) {
		/* Packet is OK, increment stats */
		datalen = rx_cqe & MLXBF_GIGE_RX_CQE_PKT_LEN_MASK;
		netdev->stats.rx_packets++;
		netdev->stats.rx_bytes += datalen;

		skb = priv->rx_skb[rx_pi_rem];

		skb_put(skb, datalen);

		skb->ip_summed = CHECKSUM_NONE; /* device did not checksum packet */

		skb->protocol = eth_type_trans(skb, netdev);
		netif_receive_skb(skb);

		/* Alloc another RX SKB for this same index */
		priv->rx_skb[rx_pi_rem] = mlxbf_gige_alloc_skb(priv, &rx_buf_dma,
							       DMA_FROM_DEVICE);
		if (!priv->rx_skb[rx_pi_rem]) {
			netdev->stats.rx_dropped++;
			return false;
		}

		*rx_wqe_addr = rx_buf_dma;
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_MAC_ERR) {
		priv->stats.rx_mac_errors++;
	} else if (rx_cqe & MLXBF_GIGE_RX_CQE_PKT_STATUS_TRUNCATED) {
		priv->stats.rx_truncate_errors++;
	}

	/* Let hardware know we've replenished one buffer */
	rx_pi++;
	writeq(rx_pi, priv->base + MLXBF_GIGE_RX_WQE_PI);

	(*rx_pkts)++;

	rx_pi_rem = rx_pi % priv->rx_q_entries;
	rx_ci = readq(priv->base + MLXBF_GIGE_RX_CQE_PACKET_CI);
	rx_ci_rem = rx_ci % priv->rx_q_entries;

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
