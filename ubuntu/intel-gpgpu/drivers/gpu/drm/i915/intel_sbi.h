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
 * Copyright © 2013-2021 Intel Corporation
 */

#ifndef _INTEL_SBI_H_
#define _INTEL_SBI_H_

#include <linux/types.h>

struct drm_i915_private;

enum intel_sbi_destination {
	SBI_ICLK,
	SBI_MPHY,
};

u32 intel_sbi_read(struct drm_i915_private *i915, u16 reg,
		   enum intel_sbi_destination destination);
void intel_sbi_write(struct drm_i915_private *i915, u16 reg, u32 value,
		     enum intel_sbi_destination destination);

#endif /* _INTEL_SBI_H_ */
