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

#ifndef __I915_VIRTUALIZATION_TYPES_H__
#define __I915_VIRTUALIZATION_TYPES_H__

/**
 * enum i915_iov_mode - I/O Virtualization mode.
 */
enum i915_iov_mode {
	I915_IOV_MODE_NONE = 1,
	I915_IOV_MODE_GVT_VGPU,
	I915_IOV_MODE_SRIOV_PF,
	I915_IOV_MODE_SRIOV_VF,
};

#endif /* __I915_VIRTUALIZATION_TYPES_H__ */
