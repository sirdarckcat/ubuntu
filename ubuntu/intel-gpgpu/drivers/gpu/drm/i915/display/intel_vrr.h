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

#ifndef __INTEL_VRR_H__
#define __INTEL_VRR_H__

#include <linux/types.h>

struct drm_connector;
struct drm_connector_state;
struct intel_atomic_state;
struct intel_crtc;
struct intel_crtc_state;
struct intel_dp;
struct intel_encoder;

bool intel_vrr_is_capable(struct drm_connector *connector);
void intel_vrr_check_modeset(struct intel_atomic_state *state);
void intel_vrr_compute_config(struct intel_crtc_state *crtc_state,
			      struct drm_connector_state *conn_state);
void intel_vrr_enable(struct intel_encoder *encoder,
		      const struct intel_crtc_state *crtc_state);
void intel_vrr_send_push(const struct intel_crtc_state *crtc_state);
bool intel_vrr_is_push_sent(const struct intel_crtc_state *crtc_state);
void intel_vrr_disable(const struct intel_crtc_state *old_crtc_state);
void intel_vrr_get_config(struct intel_crtc *crtc,
			  struct intel_crtc_state *crtc_state);
int intel_vrr_vmax_vblank_start(const struct intel_crtc_state *crtc_state);
int intel_vrr_vmin_vblank_start(const struct intel_crtc_state *crtc_state);

#endif /* __INTEL_VRR_H__ */
