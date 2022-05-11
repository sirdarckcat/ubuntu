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

#ifndef __I915_PERF_STALL_CNTR_H__
#define __I915_PERF_STALL_CNTR_H__

#include <drm/drm_file.h>

#include "i915_perf.h"

extern u32 i915_perf_stream_paranoid;

void i915_perf_stall_cntr_init(struct drm_i915_private *i915);

int i915_open_eu_stall_cntr(struct drm_i915_private *i915,
			    struct drm_i915_perf_open_param *param,
			    struct drm_file *file);

#endif /* __I915_PERF_STALL_CNTR_H__ */
