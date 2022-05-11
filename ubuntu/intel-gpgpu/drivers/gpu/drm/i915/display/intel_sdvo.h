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

#ifndef __INTEL_SDVO_H__
#define __INTEL_SDVO_H__

#include <linux/types.h>

#include "i915_reg_defs.h"

struct drm_i915_private;
enum pipe;
enum port;

bool intel_sdvo_port_enabled(struct drm_i915_private *dev_priv,
			     i915_reg_t sdvo_reg, enum pipe *pipe);
bool intel_sdvo_init(struct drm_i915_private *dev_priv,
		     i915_reg_t reg, enum port port);

#endif /* __INTEL_SDVO_H__ */
