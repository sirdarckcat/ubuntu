/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019, Mellanox Technologies. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __TRIO_REGS_H__
#define __TRIO_REGS_H__

#ifdef __ASSEMBLER__
#define _64bit(x) x
#else /* __ASSEMBLER__ */
#define _64bit(x) x ## ULL
#endif /* __ASSEMBLER */

#include <linux/types.h>

#define L3C_PROF_RD_MISS__FIRST_WORD 0x0600
#define L3C_PROF_RD_MISS__LAST_WORD 0x063c
#define L3C_PROF_RD_MISS__LENGTH 0x0040
#define L3C_PROF_RD_MISS__STRIDE 0x0004

#define L3C_PROF_RD_MISS__LOW_ORDER_SHIFT 0
#define L3C_PROF_RD_MISS__LOW_ORDER_WIDTH 5
#define L3C_PROF_RD_MISS__LOW_ORDER_RESET_VAL 11
#define L3C_PROF_RD_MISS__LOW_ORDER_RMASK 0x1f
#define L3C_PROF_RD_MISS__LOW_ORDER_MASK  0x1f

#define L3C_PROF_RD_MISS__HIGH_ORDER_SHIFT 5
#define L3C_PROF_RD_MISS__HIGH_ORDER_WIDTH 5
#define L3C_PROF_RD_MISS__HIGH_ORDER_RESET_VAL 0
#define L3C_PROF_RD_MISS__HIGH_ORDER_RMASK 0x1f
#define L3C_PROF_RD_MISS__HIGH_ORDER_MASK  0x3e0

#define L3C_PROF_RD_MISS__ALLOC_STATE_SHIFT 12
#define L3C_PROF_RD_MISS__ALLOC_STATE_WIDTH 1
#define L3C_PROF_RD_MISS__ALLOC_STATE_RESET_VAL 0
#define L3C_PROF_RD_MISS__ALLOC_STATE_RMASK 0x1
#define L3C_PROF_RD_MISS__ALLOC_STATE_MASK  0x1000

#define L3C_PROF_RD_MISS__LOW_STATE_BLK_ALLOC_SHIFT 13
#define L3C_PROF_RD_MISS__LOW_STATE_BLK_ALLOC_WIDTH 1
#define L3C_PROF_RD_MISS__LOW_STATE_BLK_ALLOC_RESET_VAL 0
#define L3C_PROF_RD_MISS__LOW_STATE_BLK_ALLOC_RMASK 0x1
#define L3C_PROF_RD_MISS__LOW_STATE_BLK_ALLOC_MASK  0x2000

#define L3C_PROF_RD_MISS__HIGH_STATE_BLK_ALLOC_SHIFT 14
#define L3C_PROF_RD_MISS__HIGH_STATE_BLK_ALLOC_WIDTH 1
#define L3C_PROF_RD_MISS__HIGH_STATE_BLK_ALLOC_RESET_VAL 0
#define L3C_PROF_RD_MISS__HIGH_STATE_BLK_ALLOC_RMASK 0x1
#define L3C_PROF_RD_MISS__HIGH_STATE_BLK_ALLOC_MASK  0x4000

#define L3C_PROF_RD_MISS__PROB_SHIFT 16
#define L3C_PROF_RD_MISS__PROB_WIDTH 16
#define L3C_PROF_RD_MISS__PROB_RESET_VAL 0
#define L3C_PROF_RD_MISS__PROB_RMASK 0xffff
#define L3C_PROF_RD_MISS__PROB_MASK  0xffff0000

#define TRIO_DEV_CTL 0x0008
#define TRIO_DEV_CTL__LENGTH 0x0001

#define TRIO_DEV_CTL__NDN_ROUTE_ORDER_SHIFT 0
#define TRIO_DEV_CTL__NDN_ROUTE_ORDER_WIDTH 1
#define TRIO_DEV_CTL__NDN_ROUTE_ORDER_RESET_VAL 0
#define TRIO_DEV_CTL__NDN_ROUTE_ORDER_RMASK 0x1
#define TRIO_DEV_CTL__NDN_ROUTE_ORDER_MASK  0x1

#define TRIO_DEV_CTL__CDN_ROUTE_ORDER_SHIFT 1
#define TRIO_DEV_CTL__CDN_ROUTE_ORDER_WIDTH 1
#define TRIO_DEV_CTL__CDN_ROUTE_ORDER_RESET_VAL 1
#define TRIO_DEV_CTL__CDN_ROUTE_ORDER_RMASK 0x1
#define TRIO_DEV_CTL__CDN_ROUTE_ORDER_MASK  0x2

#define TRIO_DEV_CTL__DDN_ROUTE_ORDER_SHIFT 2
#define TRIO_DEV_CTL__DDN_ROUTE_ORDER_WIDTH 1
#define TRIO_DEV_CTL__DDN_ROUTE_ORDER_RESET_VAL 1
#define TRIO_DEV_CTL__DDN_ROUTE_ORDER_RMASK 0x1
#define TRIO_DEV_CTL__DDN_ROUTE_ORDER_MASK  0x4

#define TRIO_DEV_CTL__DMA_RD_CA_ENA_SHIFT 3
#define TRIO_DEV_CTL__DMA_RD_CA_ENA_WIDTH 1
#define TRIO_DEV_CTL__DMA_RD_CA_ENA_RESET_VAL 1
#define TRIO_DEV_CTL__DMA_RD_CA_ENA_RMASK 0x1
#define TRIO_DEV_CTL__DMA_RD_CA_ENA_MASK  0x8

#define TRIO_DEV_CTL__L3_PROFILE_OVD_SHIFT 4
#define TRIO_DEV_CTL__L3_PROFILE_OVD_WIDTH 1
#define TRIO_DEV_CTL__L3_PROFILE_OVD_RESET_VAL 0
#define TRIO_DEV_CTL__L3_PROFILE_OVD_RMASK 0x1
#define TRIO_DEV_CTL__L3_PROFILE_OVD_MASK  0x10

#define TRIO_DEV_CTL__L3_PROFILE_VAL_SHIFT 5
#define TRIO_DEV_CTL__L3_PROFILE_VAL_WIDTH 4
#define TRIO_DEV_CTL__L3_PROFILE_VAL_RESET_VAL 0
#define TRIO_DEV_CTL__L3_PROFILE_VAL_RMASK 0xf
#define TRIO_DEV_CTL__L3_PROFILE_VAL_MASK  0x1e0

#define TRIO_DEV_CTL__WR_SLVERR_MAP_SHIFT 9
#define TRIO_DEV_CTL__WR_SLVERR_MAP_WIDTH 2
#define TRIO_DEV_CTL__WR_SLVERR_MAP_RESET_VAL 2
#define TRIO_DEV_CTL__WR_SLVERR_MAP_RMASK 0x3
#define TRIO_DEV_CTL__WR_SLVERR_MAP_MASK  0x600
#define TRIO_DEV_CTL__WR_SLVERR_MAP_VAL_OKAY 0x0
#define TRIO_DEV_CTL__WR_SLVERR_MAP_VAL_DATAERROR 0x2
#define TRIO_DEV_CTL__WR_SLVERR_MAP_VAL_NONDATAERROR 0x3

#define TRIO_DEV_CTL__WR_DECERR_MAP_SHIFT 11
#define TRIO_DEV_CTL__WR_DECERR_MAP_WIDTH 2
#define TRIO_DEV_CTL__WR_DECERR_MAP_RESET_VAL 3
#define TRIO_DEV_CTL__WR_DECERR_MAP_RMASK 0x3
#define TRIO_DEV_CTL__WR_DECERR_MAP_MASK  0x1800
#define TRIO_DEV_CTL__WR_DECERR_MAP_VAL_OKAY 0x0
#define TRIO_DEV_CTL__WR_DECERR_MAP_VAL_DATAERROR 0x2
#define TRIO_DEV_CTL__WR_DECERR_MAP_VAL_NONDATAERROR 0x3

#define TRIO_DEV_CTL__RD_SLVERR_MAP_SHIFT 13
#define TRIO_DEV_CTL__RD_SLVERR_MAP_WIDTH 2
#define TRIO_DEV_CTL__RD_SLVERR_MAP_RESET_VAL 2
#define TRIO_DEV_CTL__RD_SLVERR_MAP_RMASK 0x3
#define TRIO_DEV_CTL__RD_SLVERR_MAP_MASK  0x6000
#define TRIO_DEV_CTL__RD_SLVERR_MAP_VAL_OKAY 0x0
#define TRIO_DEV_CTL__RD_SLVERR_MAP_VAL_DATAERROR 0x2
#define TRIO_DEV_CTL__RD_SLVERR_MAP_VAL_NONDATAERROR 0x3

#define TRIO_DEV_CTL__RD_DECERR_MAP_SHIFT 15
#define TRIO_DEV_CTL__RD_DECERR_MAP_WIDTH 2
#define TRIO_DEV_CTL__RD_DECERR_MAP_RESET_VAL 3
#define TRIO_DEV_CTL__RD_DECERR_MAP_RMASK 0x3
#define TRIO_DEV_CTL__RD_DECERR_MAP_MASK  0x18000
#define TRIO_DEV_CTL__RD_DECERR_MAP_VAL_OKAY 0x0
#define TRIO_DEV_CTL__RD_DECERR_MAP_VAL_DATAERROR 0x2
#define TRIO_DEV_CTL__RD_DECERR_MAP_VAL_NONDATAERROR 0x3

#define TRIO_DEV_CTL__CDN_REQ_BUF_ENA_SHIFT 17
#define TRIO_DEV_CTL__CDN_REQ_BUF_ENA_WIDTH 1
#define TRIO_DEV_CTL__CDN_REQ_BUF_ENA_RESET_VAL 1
#define TRIO_DEV_CTL__CDN_REQ_BUF_ENA_RMASK 0x1
#define TRIO_DEV_CTL__CDN_REQ_BUF_ENA_MASK  0x20000

#define TRIO_DEV_CTL__DMA_WRQ_HWM_SHIFT 20
#define TRIO_DEV_CTL__DMA_WRQ_HWM_WIDTH 8
#define TRIO_DEV_CTL__DMA_WRQ_HWM_RESET_VAL 255
#define TRIO_DEV_CTL__DMA_WRQ_HWM_RMASK 0xff
#define TRIO_DEV_CTL__DMA_WRQ_HWM_MASK  0xff00000

#define TRIO_DEV_CTL__GTHR_DELAY_ADJ_SHIFT 28
#define TRIO_DEV_CTL__GTHR_DELAY_ADJ_WIDTH 4
#define TRIO_DEV_CTL__GTHR_DELAY_ADJ_RESET_VAL 0
#define TRIO_DEV_CTL__GTHR_DELAY_ADJ_RMASK 0xf
#define TRIO_DEV_CTL__GTHR_DELAY_ADJ_MASK  0xf0000000

#ifndef __ASSEMBLER__
__extension__
typedef union {
	struct {
		/*
		 * When 1, packets sent on the NDN will be routed x-first.
		 * When 0, packets will be routed y-first.  This setting must
		 * match the setting in the Tiles.  Devices may have
		 * additional interfaces with customized route-order settings
		 * used in addition to or instead of this field.
		 */
		u64 ndn_route_order : 1;
		/*
		 * When 1, packets sent on the CDN will be routed x-first.
		 * When 0, packets will be routed y-first.  This setting must
		 * match the setting in the Tiles.  Devices may have
		 * additional interfaces with customized route-order settings
		 * used in addition to or instead of this field.
		 */
		u64 cdn_route_order : 1;
		/*
		 * When 1, packets sent on the DDN will be routed x-first.
		 * When 0, packets will be routed y-first.  This setting must
		 * match the setting in the Tiles.  Devices may have
		 * additional interfaces with customized route-order settings
		 * used in addition to or instead of this field.
		 */
		u64 ddn_route_order : 1;
		/*
		 * When 1, the ExpCompAck flow will be used on DMA reads
		 * which allows read-data-bypass for lower latency. Must only
		 * be changed if no DMA read traffic is inflight.
		 */
		u64 dma_rd_ca_ena   : 1;
		/*
		 * For devices with DMA. When 1, the L3 cache profile will be
		 * forced to L3_PROFILE_VAL. When 0, the L3 profile is
		 * selected by the device.
		 */
		u64 l3_profile_ovd  : 1;
		/*
		 * For devices with DMA. L3 cache profile to be used when
		 * L3_PROFILE_OVD is 1.
		 */
		u64 l3_profile_val  : 4;
		/* Write response mapping for MMIO slave errors */
		u64 wr_slverr_map   : 2;
		/* Write response mapping for MMIO decode errors */
		u64 wr_decerr_map   : 2;
		/* Read response mapping for MMIO slave errors */
		u64 rd_slverr_map   : 2;
		/* Read response mapping for MMIO decode errors */
		u64 rd_decerr_map   : 2;
		/*
		 * When 1, the CDN sync FIFO is allowed to back pressure
		 * until full to avoid retries and improve performance
		 */
		u64 cdn_req_buf_ena : 1;
		/* Reserved. */
		u64 __reserved_0    : 2;
		/*
		 * For diagnostics only. Block new traffic when WRQ_INFL
		 * count exceeds this threshold. This register field does not
		 * exist in the PKA or Tile or MSS.
		 */
		u64 dma_wrq_hwm     : 8;
		/* For diagnostics only. Adjust packet gather delay on RNF */
		u64 gthr_delay_adj  : 4;
		/* Reserved. */
		u64 __reserved_1    : 32;
	};

	u64 word;
} TRIO_DEV_CTL_t;
#endif /* !defined(__ASSEMBLER__) */

#define TRIO_MMIO_ERROR_INFO 0x0608

#define TRIO_MAP_ERR_STS 0x0810

#define TRIO_TILE_PIO_CPL_ERR_STS 0x09f0

#endif /* !defined(__TRIO_REGS_H__) */
