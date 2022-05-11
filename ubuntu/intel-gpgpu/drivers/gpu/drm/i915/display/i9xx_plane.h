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

#ifndef _I9XX_PLANE_H_
#define _I9XX_PLANE_H_

#include <linux/types.h>

enum pipe;
struct drm_i915_private;
struct intel_crtc;
struct intel_initial_plane_config;
struct intel_plane;
struct intel_plane_state;

unsigned int i965_plane_max_stride(struct intel_plane *plane,
				   u32 pixel_format, u64 modifier,
				   unsigned int rotation);
int i9xx_check_plane_surface(struct intel_plane_state *plane_state);

struct intel_plane *
intel_primary_plane_create(struct drm_i915_private *dev_priv, enum pipe pipe);

void i9xx_get_initial_plane_config(struct intel_crtc *crtc,
				   struct intel_initial_plane_config *plane_config);
#endif
