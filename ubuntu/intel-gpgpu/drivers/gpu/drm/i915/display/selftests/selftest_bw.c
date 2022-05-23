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

#include "display/intel_bw.h"
#include "i915_drv.h"
#include "i915_selftest.h"
#include "intel_dram.h"
#include "selftest_display.h"

/**
 * intel_pcode_qgv_points_read_test - Test QGV point reads from pcode
 * @arg: i915 device instance
 *
 * Return 0 on success and error on fail and when dclk is zero
 */
int intel_pcode_read_qgv_points_test(void *arg)
{
	struct drm_i915_private *i915 = arg;
	struct intel_qgv_info qi;
	struct intel_qgv_point qp;
	int i, ret;
	bool fail = false;

	if (DISPLAY_VER(i915) < 11) {
		drm_info(&i915->drm, "QGV doesn't support, skipping\n");
		return 0;
	}

	intel_dram_detect(i915);
	qi.num_points = i915->dram_info.num_qgv_points;

	for (i = 0; i < qi.num_points; i++) {
		ret = icl_pcode_read_qgv_point_info(i915, &qp, i);
		if (ret) {
			drm_err(&i915->drm, "Pcode failed to read qgv point %d\n", i);
			fail = true;
		}

		if (qp.dclk == 0) {
			drm_err(&i915->drm, "DCLK set to 0 for qgv point %d\n", i);
			fail = true;
		}
	}

	if (fail)
		return -EINVAL;

	return 0;
}
