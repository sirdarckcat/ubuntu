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
/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright © 2018 Intel Corporation
 */

#include "gt/intel_gt.h"
#include "gt/intel_gt_requests.h"

#include "i915_drv.h"
#include "i915_selftest.h"

#include "igt_flush_test.h"

int igt_flush_test(struct drm_i915_private *i915)
{
	struct intel_gt *gt;
	unsigned int i;
	int ret = 0;

	for_each_gt(i915, i, gt) {
		if (intel_gt_is_wedged(gt))
			ret = -EIO;

		cond_resched();

		if (intel_gt_wait_for_idle(gt, HZ * 3) == -ETIME) {
			pr_err("%pS timed out, cancelling all further testing.\n",
			       __builtin_return_address(0));

			GEM_TRACE("%pS timed out.\n",
				__builtin_return_address(0));
			GEM_TRACE_DUMP();

			intel_gt_set_wedged(gt);
			ret = -EIO;
		}
	}

	return ret;
}
