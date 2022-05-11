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

#ifndef __VLV_DSI_PLL_H__
#define __VLV_DSI_PLL_H__

#include <linux/types.h>

enum port;
struct drm_i915_private;
struct intel_crtc_state;
struct intel_encoder;

int vlv_dsi_pll_compute(struct intel_encoder *encoder,
			struct intel_crtc_state *config);
void vlv_dsi_pll_enable(struct intel_encoder *encoder,
			const struct intel_crtc_state *config);
void vlv_dsi_pll_disable(struct intel_encoder *encoder);
u32 vlv_dsi_get_pclk(struct intel_encoder *encoder,
		     struct intel_crtc_state *config);
void vlv_dsi_reset_clocks(struct intel_encoder *encoder, enum port port);

bool bxt_dsi_pll_is_enabled(struct drm_i915_private *dev_priv);
int bxt_dsi_pll_compute(struct intel_encoder *encoder,
			struct intel_crtc_state *config);
void bxt_dsi_pll_enable(struct intel_encoder *encoder,
			const struct intel_crtc_state *config);
void bxt_dsi_pll_disable(struct intel_encoder *encoder);
u32 bxt_dsi_get_pclk(struct intel_encoder *encoder,
		     struct intel_crtc_state *config);
void bxt_dsi_reset_clocks(struct intel_encoder *encoder, enum port port);

void assert_dsi_pll_enabled(struct drm_i915_private *i915);
void assert_dsi_pll_disabled(struct drm_i915_private *i915);

#endif /* __VLV_DSI_PLL_H__ */
