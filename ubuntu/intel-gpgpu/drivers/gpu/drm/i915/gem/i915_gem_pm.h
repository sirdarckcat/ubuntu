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
 * Copyright © 2019 Intel Corporation
 */

#ifndef __I915_GEM_PM_H__
#define __I915_GEM_PM_H__

#include <linux/types.h>

struct drm_i915_private;
struct work_struct;

void i915_gem_resume_early(struct drm_i915_private *i915);
void i915_gem_resume(struct drm_i915_private *i915);

void i915_gem_idle_work_handler(struct work_struct *work);

void i915_gem_suspend(struct drm_i915_private *i915);
int i915_gem_suspend_late(struct drm_i915_private *i915);

int i915_gem_freeze(struct drm_i915_private *i915);
int i915_gem_freeze_late(struct drm_i915_private *i915);

#endif /* __I915_GEM_PM_H__ */
