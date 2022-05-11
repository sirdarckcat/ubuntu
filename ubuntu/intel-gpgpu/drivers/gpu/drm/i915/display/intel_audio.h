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

#ifndef __INTEL_AUDIO_H__
#define __INTEL_AUDIO_H__

struct drm_connector_state;
struct drm_i915_private;
struct intel_crtc_state;
struct intel_encoder;

void intel_audio_hooks_init(struct drm_i915_private *dev_priv);
void intel_audio_codec_enable(struct intel_encoder *encoder,
			      const struct intel_crtc_state *crtc_state,
			      const struct drm_connector_state *conn_state);
void intel_audio_codec_disable(struct intel_encoder *encoder,
			       const struct intel_crtc_state *old_crtc_state,
			       const struct drm_connector_state *old_conn_state);
void intel_audio_cdclk_change_pre(struct drm_i915_private *dev_priv);
void intel_audio_cdclk_change_post(struct drm_i915_private *dev_priv);
void intel_audio_init(struct drm_i915_private *dev_priv);
void intel_audio_deinit(struct drm_i915_private *dev_priv);

#endif /* __INTEL_AUDIO_H__ */
