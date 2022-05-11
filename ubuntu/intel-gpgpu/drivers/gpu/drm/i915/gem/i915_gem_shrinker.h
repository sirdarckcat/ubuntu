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

#ifndef __I915_GEM_SHRINKER_H__
#define __I915_GEM_SHRINKER_H__

#include <linux/bits.h>
#include <linux/types.h>

struct drm_i915_private;
struct i915_gem_ww_ctx;
struct mutex;
struct intel_memory_region;

/* i915_gem_shrinker.c */
unsigned long i915_gem_shrink(struct i915_gem_ww_ctx *ww,
			      struct drm_i915_private *i915,
			      unsigned long target,
			      unsigned long *nr_scanned,
			      unsigned flags);
#define I915_SHRINK_UNBOUND	BIT(0)
#define I915_SHRINK_BOUND	BIT(1)
#define I915_SHRINK_ACTIVE	BIT(2)
#define I915_SHRINK_VMAPS	BIT(3)
#define I915_SHRINK_WRITEBACK	BIT(4)

unsigned long i915_gem_shrink_all(struct drm_i915_private *i915);
void i915_gem_driver_register__shrinker(struct drm_i915_private *i915);
void i915_gem_driver_unregister__shrinker(struct drm_i915_private *i915);

#endif /* __I915_GEM_SHRINKER_H__ */
