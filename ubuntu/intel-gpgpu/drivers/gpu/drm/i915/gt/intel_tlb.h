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
 * Copyright © 2020 Intel Corporation
 */

#ifndef INTEL_TLB_H
#define INTEL_TLB_H

#include <linux/types.h>
#include "intel_gt.h"

struct i915_address_space;
struct intel_gt;

void intel_invalidate_tlb_full(struct intel_gt *gt);
void intel_invalidate_tlb_full_flush(struct intel_gt *gt);
void intel_invalidate_tlb_full_sync(struct intel_gt *gt);

void intel_invalidate_tlb_range(struct intel_gt *gt,
				struct i915_address_space *vm,
				u64 start, u64 length);
#endif /* INTEL_TLB_H */
