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
/* SPDX-License-Identifier: MIT
 *
 * Copyright © 2019 Intel Corporation
 */

#ifndef _INTEL_DSB_H
#define _INTEL_DSB_H

#include <linux/types.h>

#include "i915_reg_defs.h"

struct intel_crtc_state;
struct i915_vma;

enum dsb_id {
	INVALID_DSB = -1,
	DSB1,
	DSB2,
	DSB3,
	MAX_DSB_PER_PIPE
};

struct intel_dsb {
	enum dsb_id id;
	u32 *cmd_buf;
	struct i915_vma *vma;

	/*
	 * free_pos will point the first free entry position
	 * and help in calculating tail of command buffer.
	 */
	int free_pos;

	/*
	 * ins_start_offset will help to store start address of the dsb
	 * instuction and help in identifying the batch of auto-increment
	 * register.
	 */
	u32 ins_start_offset;
};

void intel_dsb_prepare(struct intel_crtc_state *crtc_state);
void intel_dsb_cleanup(struct intel_crtc_state *crtc_state);
void intel_dsb_reg_write(const struct intel_crtc_state *crtc_state,
			 i915_reg_t reg, u32 val);
void intel_dsb_indexed_reg_write(const struct intel_crtc_state *crtc_state,
				 i915_reg_t reg, u32 val);
void intel_dsb_commit(const struct intel_crtc_state *crtc_state);

#endif
