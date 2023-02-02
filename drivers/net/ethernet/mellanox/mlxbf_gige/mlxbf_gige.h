/* SPDX-License-Identifier: GPL-2.0-only OR Linux-OpenIB */

/* Header file for Gigabit Ethernet driver for Mellanox BlueField SoC
 * - this file contains software data structures and any chip-specific
 *   data structures (e.g. TX WQE format) that are memory resident.
 *
 * Copyright (c) 2020, Mellanox Technologies
 */

#ifndef __MLXBF_GIGE_H__
#define __MLXBF_GIGE_H__

#include <linux/irqreturn.h>
#include <linux/netdevice.h>

/* Always define this for internal Mellanox use */
#define MLXBF_GIGE_INTERNAL

#define MLXBF_GIGE_MIN_RXQ_SZ     32
#define MLXBF_GIGE_MAX_RXQ_SZ     32768
#define MLXBF_GIGE_DEFAULT_RXQ_SZ 128

#define MLXBF_GIGE_MIN_TXQ_SZ     4
#define MLXBF_GIGE_MAX_TXQ_SZ     256
#define MLXBF_GIGE_DEFAULT_TXQ_SZ 128

#define MLXBF_GIGE_DEFAULT_BUF_SZ 2048

/* Known pattern for initial state of RX buffers */
#define MLXBF_GIGE_INIT_BYTE_RX_BUF 0x10

#ifdef MLXBF_GIGE_INTERNAL
/* Number of bytes in packet to be displayed by debug routines */
#define MLXBF_GIGE_NUM_BYTES_IN_PKT_DUMP 64

/* Known pattern for fake destination MAC. This
 * value should be different from the value of
 * MLXBF_GIGE_INIT_BYTE_RX_BUF in order to track RX.
 */
#define MLXBF_GIGE_FAKE_DMAC_BYTE 0x20

/* Known pattern for fake source MAC. */
#define MLXBF_GIGE_FAKE_SMAC_BYTE 0xFF

/* Number of packets to transmit with verbose debugging on */
#define MLXBF_GIGE_MAX_TX_PKTS_VERBOSE 5

/* Default TX packet size used in 'start_tx_store' */
#define MLXBF_GIGE_DEFAULT_TX_PKT_SIZE 60
#endif /* MLXBF_GIGE_INTERNAL */

/* There are four individual MAC RX filters. Currently
 * two of them are being used: one for the broadcast MAC
 * (index 0) and one for local MAC (index 1)
 */
#define MLXBF_GIGE_BCAST_MAC_FILTER_IDX 0
#define MLXBF_GIGE_LOCAL_MAC_FILTER_IDX 1

/* Define for broadcast MAC literal */
#define BCAST_MAC_ADDR 0xFFFFFFFFFFFF

/* There are three individual interrupts:
 *   1) Errors, "OOB" interrupt line
 *   2) Receive Packet, "OOB_LLU" interrupt line
 *   3) LLU and PLU Events, "OOB_PLU" interrupt line
 */
#define MLXBF_GIGE_ERROR_INTR_IDX       0
#define MLXBF_GIGE_RECEIVE_PKT_INTR_IDX 1
#define MLXBF_GIGE_LLU_PLU_INTR_IDX     2
#define MLXBF_GIGE_PHY_INT_N            3

struct mlxbf_gige_stats {
	u64 hw_access_errors;
	u64 tx_invalid_checksums;
	u64 tx_small_frames;
	u64 tx_index_errors;
	u64 sw_config_errors;
	u64 sw_access_errors;
	u64 rx_truncate_errors;
	u64 rx_mac_errors;
	u64 rx_din_dropped_pkts;
	u64 tx_fifo_full;
	u64 rx_filter_passed_pkts;
	u64 rx_filter_discard_pkts;
};

struct mlxbf_gige {
	void __iomem *base;
	void __iomem *llu_base;
	void __iomem *plu_base;
	struct device *dev;
	struct net_device *netdev;
	struct platform_device *pdev;
	void __iomem *mdio_io;
	struct mii_bus *mdiobus;
	void __iomem *gpio_io;
	void __iomem *cause_rsh_coalesce0_io;
	void __iomem *cause_gpio_arm_coalesce0_io;
	spinlock_t gpio_lock;
	u16 rx_q_entries;
	u16 tx_q_entries;
	u64 *tx_wqe_base;
	dma_addr_t tx_wqe_base_dma;
	u64 *tx_wqe_next;
	u64 *tx_cc;
	dma_addr_t tx_cc_dma;
	dma_addr_t *rx_wqe_base;
	dma_addr_t rx_wqe_base_dma;
	u64 *rx_cqe_base;
	dma_addr_t rx_cqe_base_dma;
	u16 tx_pi;
	u16 prev_tx_ci;
	u64 error_intr_count;
	u64 rx_intr_count;
	u64 llu_plu_intr_count;
	u8 *rx_buf[MLXBF_GIGE_DEFAULT_RXQ_SZ];
	u8 *tx_buf[MLXBF_GIGE_DEFAULT_TXQ_SZ];
	int error_irq;
	int rx_irq;
	int llu_plu_irq;
	bool promisc_enabled;
	struct napi_struct napi;
	struct mlxbf_gige_stats stats;

#ifdef MLXBF_GIGE_INTERNAL
	/* Starting seed for data in loopback packets */
	u8 tx_data_seed;
#endif /* MLXBF_GIGE_INTERNAL */
};

/* Rx Work Queue Element definitions */
#define MLXBF_GIGE_RX_WQE_SZ                   8

/* Rx Completion Queue Element definitions */
#define MLXBF_GIGE_RX_CQE_SZ                   8
#define MLXBF_GIGE_RX_CQE_PKT_LEN_MASK         GENMASK(10, 0)
#define MLXBF_GIGE_RX_CQE_VALID_MASK           GENMASK(11, 11)
#define MLXBF_GIGE_RX_CQE_PKT_STATUS_MASK      GENMASK(15, 12)
#define MLXBF_GIGE_RX_CQE_PKT_STATUS_MAC_ERR   GENMASK(12, 12)
#define MLXBF_GIGE_RX_CQE_PKT_STATUS_TRUNCATED GENMASK(13, 13)
#define MLXBF_GIGE_RX_CQE_CHKSUM_MASK          GENMASK(31, 16)
#ifdef MLXBF_GIGE_INTERNAL
#define MLXBF_GIGE_RX_CQE_PKT_LEN_SHIFT        0
#define MLXBF_GIGE_RX_CQE_VALID_SHIFT          11
#define MLXBF_GIGE_RX_CQE_PKT_STATUS_SHIFT     12
#define MLXBF_GIGE_RX_CQE_CHKSUM_SHIFT         16
#endif

/* Tx Work Queue Element definitions */
#define MLXBF_GIGE_TX_WQE_SZ_QWORDS            2
#define MLXBF_GIGE_TX_WQE_SZ                   16
#define MLXBF_GIGE_TX_WQE_PKT_LEN_MASK         GENMASK(10, 0)
#define MLXBF_GIGE_TX_WQE_UPDATE_MASK          GENMASK(31, 31)
#define MLXBF_GIGE_TX_WQE_CHKSUM_LEN_MASK      GENMASK(42, 32)
#define MLXBF_GIGE_TX_WQE_CHKSUM_START_MASK    GENMASK(55, 48)
#define MLXBF_GIGE_TX_WQE_CHKSUM_OFFSET_MASK   GENMASK(63, 56)
#ifdef MLXBF_GIGE_INTERNAL
#define MLXBF_GIGE_TX_WQE_PKT_LEN_SHIFT        0
#define MLXBF_GIGE_TX_WQE_UPDATE_SHIFT         31
#define MLXBF_GIGE_TX_WQE_CHKSUM_LEN_SHIFT     32
#define MLXBF_GIGE_TX_WQE_CHKSUM_START_SHIFT   48
#define MLXBF_GIGE_TX_WQE_CHKSUM_OFFSET_SHIFT  56
#endif

/* Macro to return packet length of specified TX WQE */
#define MLXBF_GIGE_TX_WQE_PKT_LEN(tx_wqe_addr) \
	(*(tx_wqe_addr + 1) & MLXBF_GIGE_TX_WQE_PKT_LEN_MASK)

/* Tx Completion Count */
#define MLXBF_GIGE_TX_CC_SZ                    8

/* List of resources in ACPI table */
enum mlxbf_gige_res {
	MLXBF_GIGE_RES_MAC,
	MLXBF_GIGE_RES_MDIO9,
	MLXBF_GIGE_RES_GPIO0,
	MLXBF_GIGE_RES_CAUSE_RSH_COALESCE0,
	MLXBF_GIGE_RES_CAUSE_GPIO_ARM_COALESCE0,
	MLXBF_GIGE_RES_LLU,
	MLXBF_GIGE_RES_PLU
};

/* Version of register data returned by mlxbf_gige_get_regs() */
#define MLXBF_GIGE_REGS_VERSION 1

int mlxbf_gige_mdio_probe(struct platform_device *pdev,
			  struct mlxbf_gige *priv);
void mlxbf_gige_mdio_remove(struct mlxbf_gige *priv);
irqreturn_t mlxbf_gige_mdio_handle_phy_interrupt(struct mlxbf_gige *priv);

#endif /* !defined(__MLXBF_GIGE_H__) */
