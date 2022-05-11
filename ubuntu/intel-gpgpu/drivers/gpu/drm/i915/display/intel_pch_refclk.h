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

#ifndef _INTEL_PCH_REFCLK_H_
#define _INTEL_PCH_REFCLK_H_

#include <linux/types.h>

struct drm_i915_private;
struct intel_crtc_state;

void lpt_program_iclkip(const struct intel_crtc_state *crtc_state);
void lpt_disable_iclkip(struct drm_i915_private *dev_priv);
int lpt_get_iclkip(struct drm_i915_private *dev_priv);

void intel_init_pch_refclk(struct drm_i915_private *dev_priv);
void lpt_disable_clkout_dp(struct drm_i915_private *dev_priv);

#endif
