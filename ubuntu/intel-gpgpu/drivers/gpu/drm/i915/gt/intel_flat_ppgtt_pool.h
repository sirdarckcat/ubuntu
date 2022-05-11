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

#ifndef INTEL_FLAT_PPGTT_POOL_H
#define INTEL_FLAT_PPGTT_POOL_H

#include <linux/types.h>

struct intel_flat_ppgtt_pool;
struct i915_address_space;
struct i915_vma;

void intel_flat_ppgtt_pool_init_early(struct intel_flat_ppgtt_pool *fpp);
int intel_flat_ppgtt_pool_init(struct intel_flat_ppgtt_pool *fpp,
			       struct i915_address_space *vm);
void intel_flat_ppgtt_pool_park(struct intel_flat_ppgtt_pool *fpp);
void intel_flat_ppgtt_pool_fini(struct intel_flat_ppgtt_pool *fpp);

struct i915_request *
intel_flat_ppgtt_get_request(struct intel_flat_ppgtt_pool *fpp);

void intel_flat_ppgtt_allocate_requests(struct i915_vma *vma, bool clear);
void intel_flat_ppgtt_request_pool_clean(struct i915_vma *vma);

struct intel_pte_bo *
intel_flat_ppgtt_get_pte_bo(struct intel_flat_ppgtt_pool *fpp);
void intel_flat_ppgtt_put_pte_bo(struct intel_flat_ppgtt_pool *fpp,
				 struct intel_pte_bo *bo);

#endif /* INTEL_FLAT_PPGTT_POOL_H */
