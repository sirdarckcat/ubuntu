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

#ifndef __INTEL_CRT_H__
#define __INTEL_CRT_H__

#include "i915_reg_defs.h"

enum pipe;
struct drm_encoder;
struct drm_i915_private;

bool intel_crt_port_enabled(struct drm_i915_private *dev_priv,
			    i915_reg_t adpa_reg, enum pipe *pipe);
void intel_crt_init(struct drm_i915_private *dev_priv);
void intel_crt_reset(struct drm_encoder *encoder);

#endif /* __INTEL_CRT_H__ */
