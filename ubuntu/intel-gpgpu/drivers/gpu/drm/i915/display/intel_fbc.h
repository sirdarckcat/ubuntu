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

#ifndef __INTEL_FBC_H__
#define __INTEL_FBC_H__

#include <linux/types.h>

enum fb_op_origin;
struct drm_i915_private;
struct intel_atomic_state;
struct intel_crtc;
struct intel_crtc_state;
struct intel_fbc;
struct intel_plane;
struct intel_plane_state;

enum intel_fbc_id {
	INTEL_FBC_A,

	I915_MAX_FBCS,
};

int intel_fbc_atomic_check(struct intel_atomic_state *state);
bool intel_fbc_pre_update(struct intel_atomic_state *state,
			  struct intel_crtc *crtc);
void intel_fbc_post_update(struct intel_atomic_state *state,
			   struct intel_crtc *crtc);
void intel_fbc_init(struct drm_i915_private *dev_priv);
void intel_fbc_cleanup(struct drm_i915_private *dev_priv);
void intel_fbc_update(struct intel_atomic_state *state,
		      struct intel_crtc *crtc);
void intel_fbc_disable(struct intel_crtc *crtc);
void intel_fbc_global_disable(struct drm_i915_private *dev_priv);
void intel_fbc_invalidate(struct drm_i915_private *dev_priv,
			  unsigned int frontbuffer_bits,
			  enum fb_op_origin origin);
void intel_fbc_flush(struct drm_i915_private *dev_priv,
		     unsigned int frontbuffer_bits, enum fb_op_origin origin);
void intel_fbc_add_plane(struct intel_fbc *fbc, struct intel_plane *plane);
void intel_fbc_handle_fifo_underrun_irq(struct drm_i915_private *i915);
void intel_fbc_reset_underrun(struct drm_i915_private *i915);
void intel_fbc_crtc_debugfs_add(struct intel_crtc *crtc);
void intel_fbc_debugfs_register(struct drm_i915_private *i915);

#endif /* __INTEL_FBC_H__ */
