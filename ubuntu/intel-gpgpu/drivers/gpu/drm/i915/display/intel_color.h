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

#ifndef __INTEL_COLOR_H__
#define __INTEL_COLOR_H__

#include <linux/types.h>

struct intel_crtc_state;
struct intel_crtc;
struct drm_plane_state;
struct drm_property_blob;

void intel_color_init(struct intel_crtc *crtc);
int intel_color_check(struct intel_crtc_state *crtc_state);
void icl_color_commit_non_arming(const struct intel_crtc_state *crtc_state);
void intel_color_commit(const struct intel_crtc_state *crtc_state);
void intel_color_load_luts(const struct intel_crtc_state *crtc_state);
void intel_color_get_config(struct intel_crtc_state *crtc_state);
int intel_color_get_gamma_bit_precision(const struct intel_crtc_state *crtc_state);
bool intel_color_lut_equal(struct drm_property_blob *blob1,
			   struct drm_property_blob *blob2,
			   u32 gamma_mode, u32 bit_precision);
void intel_color_load_plane_luts(const struct drm_plane_state *plane_state);

#endif /* __INTEL_COLOR_H__ */
