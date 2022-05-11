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

#include "i915_drv.h"

unsigned long
i915_fence_context_timeout(const struct drm_i915_private *i915, u64 context)
{
	if (CPTCFG_DRM_I915_FENCE_TIMEOUT && context)
		return msecs_to_jiffies_timeout(CPTCFG_DRM_I915_FENCE_TIMEOUT);

	return 0;
}
