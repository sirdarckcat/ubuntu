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

#include "linux/types.h"

#ifndef INTEL_CLOS_H
#define INTEL_CLOS_H

struct drm_i915_private;
struct drm_i915_file_private;

void init_device_clos(struct drm_i915_private *dev_priv);
void uninit_device_clos(struct drm_i915_private *dev_priv);

void init_client_clos(struct drm_i915_file_private *fpriv);
void uninit_client_clos(struct drm_i915_file_private *fpriv);

int reserve_clos(struct drm_i915_file_private *fpriv, u16 *clos_index);
int free_clos(struct drm_i915_file_private *fpriv, u16 clos_index);
int reserve_cache_ways(struct drm_i915_file_private *fpriv, u16 cache_level,
			u16 clos_index, u16 *num_ways);
#endif
