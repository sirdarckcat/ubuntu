/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("LICENSE"). Unless the LICENSE provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no
 * express or implied warranties, other than those that are expressly
 * stated in the License.
 *
 */
// SPDX-License-Identifier: MIT
/*
 * Copyright © 2020 Intel Corporation
 */

#ifndef _INTEL_PAGEFAULT_H
#define _INTEL_PAGEFAULT_H

#include <linux/types.h>

struct drm_i915_gem_object;
struct drm_i915_private;
struct intel_guc;

struct recoverable_page_fault_info {
	u64 page_addr;
	u32 asid;
	u16 pdata;
	u8 vfid;
	u8 access_type;
	u8 fault_type;
	u8 fault_level;
	u8 engine_class;
	u8 engine_instance;
	u8 fault_unsuccessful;
};

enum recoverable_page_fault_type {
	FAULT_READ_NOT_PRESENT = 0x0,
	FAULT_WRITE_NOT_PRESENT = 0x1,
	FAULT_ATOMIC_NOT_PRESENT = 0x2,
	FAULT_WRITE_ACCESS_VIOLATION = 0x5,
	FAULT_ATOMIC_ACCESS_VIOLATION = 0xa,
};

const char *intel_pagefault_type2str(enum recoverable_page_fault_type type);

int intel_pagefault_process_cat_error_msg(struct intel_guc *guc,
					  const u32 *payload, u32 len);
int intel_pagefault_process_page_fault_msg(struct intel_guc *guc,
					   const u32 *payload, u32 len);
int intel_pagefault_req_process_msg(struct intel_guc *guc, const u32 *payload,
				    u32 len);
#endif
