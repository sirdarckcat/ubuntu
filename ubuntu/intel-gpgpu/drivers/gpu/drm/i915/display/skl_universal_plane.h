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

#ifndef _SKL_UNIVERSAL_PLANE_H_
#define _SKL_UNIVERSAL_PLANE_H_

#include <linux/types.h>

struct drm_i915_private;
struct intel_crtc;
struct intel_initial_plane_config;
struct intel_plane_state;

enum pipe;
enum plane_id;

struct intel_plane *
skl_universal_plane_create(struct drm_i915_private *dev_priv,
			   enum pipe pipe, enum plane_id plane_id);

void skl_get_initial_plane_config(struct intel_crtc *crtc,
				  struct intel_initial_plane_config *plane_config);

int skl_format_to_fourcc(int format, bool rgb_order, bool alpha);

int skl_calc_main_surface_offset(const struct intel_plane_state *plane_state,
				 int *x, int *y, u32 *offset);

bool icl_is_nv12_y_plane(struct drm_i915_private *dev_priv,
			 enum plane_id plane_id);
bool icl_is_hdr_plane(struct drm_i915_private *dev_priv, enum plane_id plane_id);

#endif
