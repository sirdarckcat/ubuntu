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
 * Copyright © 2020 Intel Corporation
 */

#ifndef _INTEL_FDI_H_
#define _INTEL_FDI_H_

enum pipe;
struct drm_i915_private;
struct intel_crtc;
struct intel_crtc_state;
struct intel_encoder;

int intel_fdi_link_freq(struct drm_i915_private *i915,
			const struct intel_crtc_state *pipe_config);
int ilk_fdi_compute_config(struct intel_crtc *intel_crtc,
			   struct intel_crtc_state *pipe_config);
void intel_fdi_normal_train(struct intel_crtc *crtc);
void ilk_fdi_disable(struct intel_crtc *crtc);
void ilk_fdi_pll_disable(struct intel_crtc *intel_crtc);
void ilk_fdi_pll_enable(const struct intel_crtc_state *crtc_state);
void intel_fdi_init_hook(struct drm_i915_private *dev_priv);
void hsw_fdi_link_train(struct intel_encoder *encoder,
			const struct intel_crtc_state *crtc_state);
void hsw_fdi_disable(struct intel_encoder *encoder);
void intel_fdi_pll_freq_update(struct drm_i915_private *i915);

void intel_fdi_link_train(struct intel_crtc *crtc,
			  const struct intel_crtc_state *crtc_state);

void assert_fdi_tx_enabled(struct drm_i915_private *i915, enum pipe pipe);
void assert_fdi_tx_disabled(struct drm_i915_private *i915, enum pipe pipe);
void assert_fdi_rx_enabled(struct drm_i915_private *i915, enum pipe pipe);
void assert_fdi_rx_disabled(struct drm_i915_private *i915, enum pipe pipe);
void assert_fdi_tx_pll_enabled(struct drm_i915_private *i915, enum pipe pipe);
void assert_fdi_rx_pll_enabled(struct drm_i915_private *i915, enum pipe pipe);
void assert_fdi_rx_pll_disabled(struct drm_i915_private *i915, enum pipe pipe);

#endif
