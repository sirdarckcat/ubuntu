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
 * Copyright © 2019 - 2022 Intel Corporation
 */

#ifndef _INTEL_IAF_H_
#define _INTEL_IAF_H_

/*
 * Define the maximum number of devices instances based on the amount of
 * FID space.
 *
 * XARRAY limits are "inclusive", but using this value as a range check
 * outside of xarray, makes the exclusive upper bound a little easier to
 * deal with.
 *
 * I.e.:
 * [0 - 256)
 *
 * Less than HW supports, but more than will be currently possible.
 *
 */
#define MAX_DEVICE_COUNT 256

/* Fixed Device Physical Address (DPA) size for a device/package (in GB) */
#define MAX_DPA_SIZE 128

struct drm_i915_private;

void intel_iaf_init_early(struct drm_i915_private *i915);
void intel_iaf_init_mmio(struct drm_i915_private *i915);
void intel_iaf_init(struct drm_i915_private *i915);
void intel_iaf_init_mfd(struct drm_i915_private *i915);
void intel_iaf_remove(struct drm_i915_private *i915);
int intel_iaf_pcie_error_notify(struct drm_i915_private *i915);
int intel_iaf_mapping_get(struct drm_i915_private *i915);
int intel_iaf_mapping_put(struct drm_i915_private *i915);

#endif
