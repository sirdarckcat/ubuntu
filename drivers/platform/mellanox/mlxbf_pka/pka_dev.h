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

#ifndef __PKA_DEV_H__
#define __PKA_DEV_H__

#include <linux/types.h>
#include <linux/vfio.h>

#include "pka_config.h"
#include "pka_cpu.h"
#include "pka_debug.h"
#include "pka_firmware.h"
#include "pka_ioctl.h"
#include "pka_mmio.h"
#include "pka_ring.h"

/* Device resource structure */
struct pka_dev_res_t {
	/* (iore)map-ped version of addr, for driver internal use */
	void    *ioaddr;
	/* base address of the device's resource */
	uint64_t base;
	/* size of IO */
	uint64_t size;
	/* type of resource addr points to */
	uint8_t  type;
	/* status of the resource */
	int8_t   status;
	/* name of the resource */
	char    *name;
};

/* defines for pka_dev_res->type */
#define PKA_DEV_RES_TYPE_MEM            1   /* resource type is memory */
#define PKA_DEV_RES_TYPE_REG            2   /* resource type is register */

/* defines for pka_dev_res->status */
#define PKA_DEV_RES_STATUS_MAPPED       1  /* the resource is (iore)-mapped */
#define PKA_DEV_RES_STATUS_UNMAPPED    -1  /* the resource is unmapped */

/* PKA Ring resources structure */
struct pka_dev_ring_res_t {
	struct pka_dev_res_t   info_words;      /* ring information words */
	struct pka_dev_res_t   counters;        /* ring counters */
	struct pka_dev_res_t   window_ram;      /* window RAM */
};

/* PKA Ring structure */
struct pka_dev_ring_t {
	/* ring identifier. */
	uint32_t                       ring_id;
	/* pointer to the shim associated to the ring. */
	struct pka_dev_shim_t         *shim;
	/* number of ring ressources. */
	uint32_t                       resources_num;
	/* ring resources. */
	struct pka_dev_ring_res_t      resources;
	/* ring information. */
	struct pka_dev_hw_ring_info_t *ring_info;
	/* number of command descriptors. */
	uint32_t                       num_cmd_desc;
	/* status of the ring. */
	int8_t                         status;
};

/* defines for pka_dev_ring->status */
#define PKA_DEV_RING_STATUS_UNDEFINED   -1
#define PKA_DEV_RING_STATUS_INITIALIZED  1
#define PKA_DEV_RING_STATUS_READY        2
#define PKA_DEV_RING_STATUS_BUSY         3
#define PKA_DEV_RING_STATUS_FINALIZED    4

/* PKA Shim resources structure */
struct pka_dev_shim_res_t {
	struct pka_dev_res_t buffer_ram;      /* buffer RAM */
	struct pka_dev_res_t master_prog_ram; /* master program RAM */
	struct pka_dev_res_t master_seq_ctrl; /* master controller CSR */
	struct pka_dev_res_t aic_csr;         /* interrupt controller CSRs */
	struct pka_dev_res_t trng_csr;        /* TRNG module CSRs */
};

#define PKA_DEV_SHIM_RES_CNT         5  /* Number of PKA device resources */

/* Platform global shim resource information */
struct pka_dev_gbl_shim_res_info_t {
	struct pka_dev_res_t *res_tbl[PKA_DEV_SHIM_RES_CNT];
	uint8_t               res_cnt;
};

/* PKA Shim structure */
struct pka_dev_shim_t {
	/* shim base address */
	uint64_t                  base;
	/* shim io memory size */
	uint64_t                  size;
	/* TRNG error cycle */
	uint64_t                  trng_err_cycle;
	/* shim identifier */
	uint32_t                  shim_id;
	/* Number of supported rings (hw specific) */
	uint32_t                  rings_num;
	/* pointer to rings which belong to the shim. */
	struct pka_dev_ring_t   **rings;
	/* specify the priority in which rings are handled. */
	uint8_t                   ring_priority;
	/*
	 * indicates whether the result ring delivers results strictly
	 * in-order.
	 */
	uint8_t                   ring_type;
	/* shim resources */
	struct pka_dev_shim_res_t resources;
	/*
	 * Window RAM mode. if non-zero, the splitted window RAM scheme
	 * is used.
	 */
	uint8_t                    window_ram_split;
	/* Number of active rings (rings in busy state) */
	uint32_t                   busy_ring_num;
	/* Whether the TRNG engine is enabled. */
	uint8_t                    trng_enabled;
	/* status of the shim */
	int8_t                     status;
};

/* defines for pka_dev_shim->status */
#define PKA_SHIM_STATUS_UNDEFINED          -1
#define PKA_SHIM_STATUS_CREATED             1
#define PKA_SHIM_STATUS_INITIALIZED         2
#define PKA_SHIM_STATUS_RUNNING             3
#define PKA_SHIM_STATUS_STOPPED             4
#define PKA_SHIM_STATUS_FINALIZED           5

/* defines for pka_dev_shim->window_ram_split */
/* window RAM is splitted into 4x16KB blocks */
#define PKA_SHIM_WINDOW_RAM_SPLIT_ENABLED   1
/* window RAM is not splitted and occupies 64KB */
#define PKA_SHIM_WINDOW_RAM_SPLIT_DISABLED  2

/* defines for pka_dev_shim->trng_enabled */
#define PKA_SHIM_TRNG_ENABLED               1
#define PKA_SHIM_TRNG_DISABLED              0

/* Platform global configuration structure */
struct pka_dev_gbl_config_t {
	/* number of registered PKA shims. */
	uint32_t                dev_shims_cnt;
	/* number of registered Rings. */
	uint32_t                dev_rings_cnt;
	/* table of registered PKA shims. */
	struct pka_dev_shim_t  *dev_shims[PKA_MAX_NUM_IO_BLOCKS];
	/* table of registered Rings. */
	struct pka_dev_ring_t  *dev_rings[PKA_MAX_NUM_RINGS];
};

extern struct pka_dev_gbl_config_t pka_gbl_config;

/*
 * Ring getter for pka_dev_gbl_config_t structure which holds all system
 * global configuration. This configuration is shared and common to kernel
 * device driver associated with PKA hardware.
 */
struct pka_dev_ring_t *pka_dev_get_ring(uint32_t ring_id);

/*
 * Shim getter for pka_dev_gbl_config_t structure which holds all system
 * global configuration. This configuration is shared and common to kernel
 * device driver associated with PKA hardware.
 */
struct pka_dev_shim_t *pka_dev_get_shim(uint32_t shim_id);

/*
 * Register a Ring. This function initializes a Ring and configures its
 * related resources, and returns a pointer to that ring.
 */
struct pka_dev_ring_t *pka_dev_register_ring(uint32_t ring_id,
					     uint32_t shim_id);

/* Unregister a Ring. */
int pka_dev_unregister_ring(struct pka_dev_ring_t *ring);

/*
 * Register PKA IO block. This function initializes a shim and configures its
 * related resources, and returns a pointer to that ring.
 */
struct pka_dev_shim_t *pka_dev_register_shim(uint32_t shim_id,
					     uint64_t shim_base,
					     uint64_t shim_size,
					     uint8_t shim_fw_id);

/* Unregister PKA IO block. */
int pka_dev_unregister_shim(struct pka_dev_shim_t *shim);

/* Reset a Ring. */
int pka_dev_reset_ring(struct pka_dev_ring_t *ring);

/*
 * Read data from the TRNG. Drivers can fill up to 'cnt' bytes of data into
 * the buffer 'data'. The buffer 'data' is aligned for any type and 'cnt' is
 * a multiple of 4.
 */
int pka_dev_trng_read(struct pka_dev_shim_t *shim,
		      uint32_t              *data,
		      uint32_t               cnt);

/* Return true if the TRNG engine is enabled, false if not. */
bool pka_dev_has_trng(struct pka_dev_shim_t *shim);

/*
 * Open the file descriptor associated with ring. It returns an integer value,
 * which is used to refer to the file. If un-successful, it returns a negative
 * error.
 */
int pka_dev_open_ring(uint32_t ring_id);

/*
 * Close the file descriptor associated with ring. The function returns 0 if
 * successful, negative value to indicate an error.
 */
int pka_dev_close_ring(uint32_t ring_id);

#endif  /* __PKA_DEV_H__ */
