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

#ifndef __INTEL_LVDS_H__
#define __INTEL_LVDS_H__

#include <linux/types.h>

#include "i915_reg_defs.h"

enum pipe;
struct drm_i915_private;

bool intel_lvds_port_enabled(struct drm_i915_private *dev_priv,
			     i915_reg_t lvds_reg, enum pipe *pipe);
void intel_lvds_init(struct drm_i915_private *dev_priv);
struct intel_encoder *intel_get_lvds_encoder(struct drm_i915_private *dev_priv);
bool intel_is_dual_link_lvds(struct drm_i915_private *dev_priv);

#endif /* __INTEL_LVDS_H__ */
