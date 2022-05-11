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

#ifndef __INTEL_FB_PIN_H__
#define __INTEL_FB_PIN_H__

#include <linux/types.h>

struct drm_framebuffer;
struct i915_vma;
struct intel_plane_state;
struct i915_ggtt_view;

struct i915_vma *
intel_pin_and_fence_fb_obj(struct drm_framebuffer *fb,
			   bool phys_cursor,
			   const struct i915_ggtt_view *view,
			   bool uses_fence,
			   unsigned long *out_flags);

void intel_unpin_fb_vma(struct i915_vma *vma, unsigned long flags);

int intel_plane_pin_fb(struct intel_plane_state *plane_state);
void intel_plane_unpin_fb(struct intel_plane_state *old_plane_state);

#endif
