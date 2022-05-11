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

#ifndef _INTEL_PCH_DISPLAY_H_
#define _INTEL_PCH_DISPLAY_H_

#include <linux/types.h>

enum pipe;
struct drm_i915_private;
struct intel_atomic_state;
struct intel_crtc;
struct intel_crtc_state;
struct intel_link_m_n;

bool intel_has_pch_trancoder(struct drm_i915_private *i915,
			     enum pipe pch_transcoder);
enum pipe intel_crtc_pch_transcoder(struct intel_crtc *crtc);

void ilk_pch_pre_enable(struct intel_atomic_state *state,
			struct intel_crtc *crtc);
void ilk_pch_enable(struct intel_atomic_state *state,
		    struct intel_crtc *crtc);
void ilk_pch_disable(struct intel_atomic_state *state,
		     struct intel_crtc *crtc);
void ilk_pch_post_disable(struct intel_atomic_state *state,
			  struct intel_crtc *crtc);
void ilk_pch_get_config(struct intel_crtc_state *crtc_state);

void lpt_pch_enable(struct intel_atomic_state *state,
		    struct intel_crtc *crtc);
void lpt_pch_disable(struct intel_atomic_state *state,
		     struct intel_crtc *crtc);
void lpt_pch_get_config(struct intel_crtc_state *crtc_state);

void intel_pch_transcoder_get_m1_n1(struct intel_crtc *crtc,
				    struct intel_link_m_n *m_n);
void intel_pch_transcoder_get_m2_n2(struct intel_crtc *crtc,
				    struct intel_link_m_n *m_n);

void intel_pch_sanitize(struct drm_i915_private *i915);

#endif
