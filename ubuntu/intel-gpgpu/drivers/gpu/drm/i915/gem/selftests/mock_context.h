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
 * Copyright © 2016 Intel Corporation
 */

#ifndef __MOCK_CONTEXT_H
#define __MOCK_CONTEXT_H

struct file;
struct drm_i915_private;
struct intel_engine_cs;
struct intel_gt;

void mock_init_contexts(struct drm_i915_private *i915);

struct i915_gem_context *
mock_context(struct drm_i915_private *i915,
	     const char *name);

void mock_context_close(struct i915_gem_context *ctx);

struct i915_gem_context *
live_gt_context(struct intel_gt *gt, struct file *file);
struct i915_gem_context *
live_context(struct drm_i915_private *i915, struct file *file);

struct i915_gem_context *
live_context_for_engine(struct intel_engine_cs *engine, struct file *file);

struct i915_gem_context *kernel_context(struct drm_i915_private *i915);
void kernel_context_close(struct i915_gem_context *ctx);

#endif /* !__MOCK_CONTEXT_H */
