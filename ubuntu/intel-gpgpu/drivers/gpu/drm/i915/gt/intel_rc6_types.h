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
 * Copyright © 2019 Intel Corporation
 */

#ifndef INTEL_RC6_TYPES_H
#define INTEL_RC6_TYPES_H

#include <linux/spinlock.h>
#include <linux/types.h>

#include "intel_engine_types.h"

struct drm_i915_gem_object;

struct intel_rc6 {
	u64 prev_hw_residency[4];
	u64 cur_residency[4];

	u32 ctl_enable;

	struct drm_i915_gem_object *pctx;

	struct drm_i915_gem_object *dfd_restore_obj;
	u32 *dfd_restore_buf;

	bool supported : 1;
	bool enabled : 1;
	bool manual : 1;
	bool wakeref : 1;
};

#endif /* INTEL_RC6_TYPES_H */
