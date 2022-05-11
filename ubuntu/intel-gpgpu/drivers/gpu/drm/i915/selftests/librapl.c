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

#include <asm/msr.h>

#include "i915_drv.h"
#include "i915_hwmon.h"
#include "librapl.h"

u64 librapl_energy_uJ(struct drm_i915_private *i915)
{
	unsigned long long power;
	u32 units;
	u64 energy_uJ = 0;

	if (IS_DGFX(i915)) {
		if (i915_energy_status_get(i915, &energy_uJ))
			return 0;

	} else {
		if (rdmsrl_safe(MSR_RAPL_POWER_UNIT, &power))
			return 0;

		units = (power & 0x1f00) >> 8;

		if (rdmsrl_safe(MSR_PP1_ENERGY_STATUS, &power))
			return 0;

		energy_uJ = (1000000 * power) >> units; /* convert to uJ */
	}
	return energy_uJ;

}
