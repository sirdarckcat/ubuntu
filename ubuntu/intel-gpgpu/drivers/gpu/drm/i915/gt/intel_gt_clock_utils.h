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

#ifndef __INTEL_GT_CLOCK_UTILS_H__
#define __INTEL_GT_CLOCK_UTILS_H__

#include <linux/types.h>

struct intel_gt;

void intel_gt_init_clock_frequency(struct intel_gt *gt);
void intel_gt_fini_clock_frequency(struct intel_gt *gt);

#if IS_ENABLED(CPTCFG_DRM_I915_DEBUG_GEM)
void intel_gt_check_clock_frequency(const struct intel_gt *gt);
#else
static inline void intel_gt_check_clock_frequency(const struct intel_gt *gt) {}
#endif

u64 intel_gt_clock_interval_to_ns(const struct intel_gt *gt, u64 count);
u64 intel_gt_pm_interval_to_ns(const struct intel_gt *gt, u64 count);

u64 intel_gt_ns_to_clock_interval(const struct intel_gt *gt, u64 ns);
u64 intel_gt_ns_to_pm_interval(const struct intel_gt *gt, u64 ns);

#endif /* __INTEL_GT_CLOCK_UTILS_H__ */
