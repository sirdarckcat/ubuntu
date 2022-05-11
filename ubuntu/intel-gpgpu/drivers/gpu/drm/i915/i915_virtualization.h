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
 * Copyright © 2022 Intel Corporation
 */

#ifndef __I915_VIRTUALIZATION_H__
#define __I915_VIRTUALIZATION_H__

#include <linux/build_bug.h>

#include "i915_gem.h"
#include "i915_virtualization_types.h"

static inline const char *i915_iov_mode_to_string(enum i915_iov_mode mode)
{
	switch (mode) {
	case I915_IOV_MODE_NONE:
		return "non virtualized";
	case I915_IOV_MODE_GVT_VGPU:
		return "GVT VGPU";
	case I915_IOV_MODE_SRIOV_PF:
		return "SR-IOV PF";
	case I915_IOV_MODE_SRIOV_VF:
		return "SR-IOV VF";
	default:
		return "<invalid>";
	}
}

#define IS_IOV_ACTIVE(i915) (IOV_MODE(i915) != I915_IOV_MODE_NONE)

#endif /* __I915_VIRTUALIZATION_H__ */
