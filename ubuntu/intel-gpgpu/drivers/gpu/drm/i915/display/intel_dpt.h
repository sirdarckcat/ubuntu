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
 * Copyright © 2021 Intel Corporation
 */

#ifndef __INTEL_DPT_H__
#define __INTEL_DPT_H__

struct drm_i915_private;

struct i915_address_space;
struct i915_vma;
struct intel_framebuffer;

void intel_dpt_destroy(struct i915_address_space *vm);
struct i915_vma *intel_dpt_pin(struct i915_address_space *vm);
void intel_dpt_unpin(struct i915_address_space *vm);
void intel_dpt_suspend(struct drm_i915_private *i915);
void intel_dpt_resume(struct drm_i915_private *i915);
struct i915_address_space *
intel_dpt_create(struct intel_framebuffer *fb);

#endif /* __INTEL_DPT_H__ */
