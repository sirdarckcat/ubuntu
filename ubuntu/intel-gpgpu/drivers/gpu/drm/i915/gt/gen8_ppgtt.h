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
/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2020 Intel Corporation
 */

#ifndef __GEN8_PPGTT_H__
#define __GEN8_PPGTT_H__

#include <linux/kernel.h>

struct i915_address_space;
struct intel_gt;
struct drm_mm_node;
enum i915_cache_level;

struct i915_ppgtt *gen8_ppgtt_create(struct intel_gt *gt, u32 flags);
u64 gen8_ggtt_pte_encode(dma_addr_t addr,
			 enum i915_cache_level level,
			 u32 flags);

int intel_flat_lmem_ppgtt_init(struct i915_address_space *vm,
			 struct drm_mm_node *node);
void intel_flat_lmem_ppgtt_fini(struct i915_address_space *vm,
			  struct drm_mm_node *node);
void gen12_init_fault_scratch(struct i915_address_space *vm, u64 start, u64 length,
			      bool valid);

#endif
