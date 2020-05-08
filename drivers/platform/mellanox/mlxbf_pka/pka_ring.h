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

#ifndef __PKA_RING_H__
#define __PKA_RING_H__

#include <linux/types.h>

/* Bluefield PKA command descriptor. */
struct pka_ring_hw_cmd_desc_t { /* 64 bytes long. 64 bytes aligned */
	uint64_t pointer_a;
	uint64_t pointer_b;
	uint64_t pointer_c;
	uint64_t pointer_d;
	uint64_t tag;
	uint64_t pointer_e;
#ifdef __AARCH64EB__
	uint64_t linked         : 1;
	uint64_t driver_status  : 2;
	uint64_t odd_powers     : 5;    /* shiftCnt for shift ops  */
	uint64_t kdk            : 2;    /* Key Decryption Key number */
	uint64_t encrypted_mask : 6;
	uint64_t rsvd_3         : 8;
	uint64_t command        : 8;
	uint64_t rsvd_2         : 5;
	uint64_t length_b       : 9;
	uint64_t output_attr    : 1;
	uint64_t input_attr     : 1;
	uint64_t rsvd_1         : 5;
	uint64_t length_a       : 9;
	uint64_t rsvd_0         : 2;
#else
	uint64_t rsvd_0         : 2;
	uint64_t length_a       : 9;
	uint64_t rsvd_1         : 5;
	uint64_t input_attr     : 1;
	uint64_t output_attr    : 1;
	uint64_t length_b       : 9;
	uint64_t rsvd_2         : 5;
	uint64_t command        : 8;
	uint64_t rsvd_3         : 8;
	uint64_t encrypted_mask : 6;
	uint64_t kdk            : 2;    /* Key Decryption Key number */
	uint64_t odd_powers     : 5;    /* shiftCnt for shift ops */
	uint64_t driver_status  : 2;
	uint64_t linked         : 1;
#endif
	uint64_t rsvd_4;
};

#define CMD_DESC_SIZE sizeof(struct pka_ring_hw_cmd_desc_t) /* Must be 64 */

/* Bluefield PKA result descriptor. */
struct pka_ring_hw_rslt_desc_t { /* 64 bytes long. 64 bytes aligned */
	uint64_t pointer_a;
	uint64_t pointer_b;
	uint64_t pointer_c;
	uint64_t pointer_d;
	uint64_t tag;
#ifdef __AARCH64EB__
	uint64_t rsvd_5                 : 13;
	uint64_t cmp_result             : 3;
	uint64_t modulo_is_0            : 1;
	uint64_t rsvd_4                 : 2;
	uint64_t modulo_msw_offset      : 11;
	uint64_t rsvd_3                 : 2;
	uint64_t rsvd_2                 : 11;
	uint64_t main_result_msb_offset : 5;
	uint64_t result_is_0            : 1;
	uint64_t rsvd_1                 : 2;
	uint64_t main_result_msw_offset : 11;
	uint64_t rsvd_0                 : 2;
	uint64_t linked         : 1;
	uint64_t driver_status  : 2;    /* Always written to 0 */
	uint64_t odd_powers     : 5;    /* shiftCnt for shift ops */
	uint64_t kdk            : 2;    /* Key Decryption Key number */
	uint64_t encrypted_mask : 6;
	uint64_t result_code    : 8;
	uint64_t command        : 8;
	uint64_t rsvd_8         : 5;
	uint64_t length_b       : 9;
	uint64_t output_attr    : 1;
	uint64_t input_attr     : 1;
	uint64_t rsvd_7         : 5;
	uint64_t length_a       : 9;
	uint64_t rsvd_6         : 2;
#else
	uint64_t rsvd_0                 : 2;
	uint64_t main_result_msw_offset : 11;
	uint64_t rsvd_1                 : 2;
	uint64_t result_is_0            : 1;
	uint64_t main_result_msb_offset : 5;
	uint64_t rsvd_2                 : 11;
	uint64_t rsvd_3                 : 2;
	uint64_t modulo_msw_offset      : 11;
	uint64_t rsvd_4                 : 2;
	uint64_t modulo_is_0            : 1;
	uint64_t cmp_result             : 3;
	uint64_t rsvd_5                 : 13;
	uint64_t rsvd_6         : 2;
	uint64_t length_a       : 9;
	uint64_t rsvd_7         : 5;
	uint64_t input_attr     : 1;
	uint64_t output_attr    : 1;
	uint64_t length_b       : 9;
	uint64_t rsvd_8         : 5;
	uint64_t command        : 8;
	uint64_t result_code    : 8;
	uint64_t encrypted_mask : 6;
	uint64_t kdk            : 2;    /* Key Decryption Key number */
	uint64_t odd_powers     : 5;    /* shiftCnt for shift ops */
	uint64_t driver_status  : 2;    /* Always written to 0 */
	uint64_t linked         : 1;
#endif
	uint64_t rsvd_9;
};

#define RESULT_DESC_SIZE sizeof(struct pka_ring_hw_rslt_desc_t) /* Must be 64 */

#endif /* __PKA_RING_H__ */
