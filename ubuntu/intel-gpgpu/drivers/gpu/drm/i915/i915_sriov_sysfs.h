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
 * Copyright © 2022 Intel Corporation
 */

#ifndef __I915_SRIOV_SYSFS_H__
#define __I915_SRIOV_SYSFS_H__

#include "i915_sriov_sysfs_types.h"

int i915_sriov_sysfs_setup(struct drm_i915_private *i915);
void i915_sriov_sysfs_teardown(struct drm_i915_private *i915);
void i915_sriov_sysfs_update_links(struct drm_i915_private *i915, bool add);

struct drm_i915_private *sriov_kobj_to_i915(struct i915_sriov_kobj *kobj);
struct drm_i915_private *sriov_ext_kobj_to_i915(struct i915_sriov_ext_kobj *kobj);

#endif /* __I915_SRIOV_SYSFS_H__ */
