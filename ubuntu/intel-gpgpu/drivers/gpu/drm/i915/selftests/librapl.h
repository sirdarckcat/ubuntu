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

#ifndef SELFTEST_LIBRAPL_H
#define SELFTEST_LIBRAPL_H

#include <linux/types.h>

struct drm_i915_private;

u64 librapl_energy_uJ(struct drm_i915_private *i915);

static inline bool librapl_supported(struct drm_i915_private *i915)
{
	return librapl_energy_uJ(i915);
}

#endif /* SELFTEST_LIBRAPL_H */
