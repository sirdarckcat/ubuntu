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

#ifndef __INTEL_HWMON_H__
#define __INTEL_HWMON_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include "i915_reg.h"

/* For definition of max number of GTs */
#include "intel_memory_region.h"

struct drm_i915_private;

struct i915_hwmon_reg {
	i915_reg_t pkg_power_sku_unit;
	i915_reg_t pkg_power_sku;
	i915_reg_t pkg_rapl_limit;
	i915_reg_t energy_status_all;
	i915_reg_t energy_status_tile;
};

struct i915_energy_info {
	u32 energy_counter_overflow;
	u32 energy_counter_prev;
};

struct i915_hwmon_drvdata {
	struct i915_hwmon *dd_hwmon;
	struct intel_uncore *dd_uncore;
	struct device *dd_hwmon_dev;
	struct i915_energy_info dd_ei;	/*  Energy info for energy1_input */
	char dd_name[12];
	int dd_gtix;
};

struct i915_hwmon {
	struct i915_hwmon_drvdata ddat;

	struct i915_hwmon_drvdata ddat_gt[I915_MAX_TILES];

	struct mutex hwmon_lock;	/* counter overflow logic and rmw */

	struct i915_hwmon_reg rg;

	u32 power_max_initial_value;

	int scl_shift_power;
	int scl_shift_energy;
};

void i915_hwmon_register(struct drm_i915_private *i915);
void i915_hwmon_unregister(struct drm_i915_private *i915);

int i915_energy_status_get(struct drm_i915_private *i915, u64 *energy);
#endif
