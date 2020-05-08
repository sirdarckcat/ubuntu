/* SPDX-License-Identifier: GPL-2.0
 *
 *  Copyright (C) 2018 Mellanox Techologies, Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License v2.0 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __PKA_IOCTL_H__
#define __PKA_IOCTL_H__

#include <linux/types.h>
#include <linux/ioctl.h>

#define PKA_IOC_TYPE 0xB7

/*
 * PKA_VFIO_GET_REGION_INFO - _IORW(PKA_IOC_TYPE, 0x0, pka_dev_region_info_t)
 *
 * Retrieve information about a device region. This is intended to describe
 * MMIO, I/O port, as well as bus specific regions (ex. PCI config space).
 * Zero sized regions may be used to describe unimplemented regions.
 * Return: 0 on success, -errno on failure.
 */
struct pka_dev_region_info_t {
	uint32_t reg_index;     /* register index. */
	uint64_t reg_size;      /* register size in bytes. */
	uint64_t reg_offset;    /* register offset from start of device fd. */
	uint32_t mem_index;     /* Memory index. */
	uint64_t mem_size;      /* Memory size (bytes). */
	uint64_t mem_offset;    /* Memeory offset from start of device fd. */
};
#define PKA_VFIO_GET_REGION_INFO \
	_IOWR(PKA_IOC_TYPE, 0x0, struct pka_dev_region_info_t)

/*
 * PKA_VFIO_GET_RING_INFO _IORW(PKA_IOC_TYPE, 0x1, struct pka_dev_ring_info_t)
 *
 * Retrieve information about a ring. This is intended to describe ring
 * information words located in PKA_BUFFER_RAM. Ring information includes
 * base addresses, size and statistics.
 * Return: 0 on success, -errno on failure.
 */
struct  pka_dev_hw_ring_info_t { /* Bluefield specific ring information */

	/* Base address of the command descriptor ring. */
	uint64_t cmmd_base;

	/* Base address of the result descriptor ring. */
	uint64_t rslt_base;

	/*
	 * Size of a command ring in number of descriptors, minus 1.
	 * Minimum value is 0 (for 1 descriptor); maximum value is
	 * 65535 (for 64K descriptors).
	 */
	uint16_t size;

	/*
	 * This field specifies the size (in 32-bit words) of the
	 * space that PKI command and result descriptor occupies on
	 * the Host.
	 */
	uint16_t host_desc_size : 10;

	/*
	 * Indicates whether the result ring delivers results strictly
	 * in-order ('1') or that result descriptors are written to the
	 * result ring as soon as they become available, so out-of-order
	 * ('0').
	 */
	uint8_t  in_order       : 1;

	/* Read pointer of the command descriptor ring. */
	uint16_t cmmd_rd_ptr;

	/* Write pointer of the result descriptor ring. */
	uint16_t rslt_wr_ptr;

	/* Read statistics of the command descriptor ring. */
	uint16_t cmmd_rd_stats;

	/* Write statistics of the result descriptor ring. */
	uint16_t rslt_wr_stats;
};
#define PKA_VFIO_GET_RING_INFO \
	_IOWR(PKA_IOC_TYPE, 0x1, struct pka_dev_hw_ring_info_t)

#endif /* __PKA_IOCTL_H__ */
