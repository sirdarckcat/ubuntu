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

#ifndef __INTEL_PSR_H__
#define __INTEL_PSR_H__

#include <linux/types.h>

enum fb_op_origin;
struct drm_connector;
struct drm_connector_state;
struct drm_i915_private;
struct intel_atomic_state;
struct intel_crtc;
struct intel_crtc_state;
struct intel_dp;
struct intel_encoder;
struct intel_plane;
struct intel_plane_state;

void intel_psr_init_dpcd(struct intel_dp *intel_dp);
void intel_psr_pre_plane_update(struct intel_atomic_state *state,
				struct intel_crtc *crtc);
void intel_psr_post_plane_update(const struct intel_atomic_state *state);
void intel_psr_disable(struct intel_dp *intel_dp,
		       const struct intel_crtc_state *old_crtc_state);
int intel_psr_debug_set(struct intel_dp *intel_dp, u64 value);
void intel_psr_invalidate(struct drm_i915_private *dev_priv,
			  unsigned frontbuffer_bits,
			  enum fb_op_origin origin);
void intel_psr_flush(struct drm_i915_private *dev_priv,
		     unsigned frontbuffer_bits,
		     enum fb_op_origin origin);
void intel_psr_init(struct intel_dp *intel_dp);
void intel_psr_compute_config(struct intel_dp *intel_dp,
			      struct intel_crtc_state *crtc_state,
			      struct drm_connector_state *conn_state);
void intel_psr_get_config(struct intel_encoder *encoder,
			  struct intel_crtc_state *pipe_config);
void intel_psr_irq_handler(struct intel_dp *intel_dp, u32 psr_iir);
void intel_psr_short_pulse(struct intel_dp *intel_dp);
void intel_psr_wait_for_idle(const struct intel_crtc_state *new_crtc_state);
bool intel_psr_enabled(struct intel_dp *intel_dp);
int intel_psr2_sel_fetch_update(struct intel_atomic_state *state,
				struct intel_crtc *crtc);
void intel_psr2_program_trans_man_trk_ctl(const struct intel_crtc_state *crtc_state);
void intel_psr2_program_plane_sel_fetch(struct intel_plane *plane,
					const struct intel_crtc_state *crtc_state,
					const struct intel_plane_state *plane_state,
					int color_plane);
void intel_psr2_disable_plane_sel_fetch(struct intel_plane *plane,
					const struct intel_crtc_state *crtc_state);
void intel_psr_pause(struct intel_dp *intel_dp);
void intel_psr_resume(struct intel_dp *intel_dp);

#endif /* __INTEL_PSR_H__ */
