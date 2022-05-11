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

#ifndef INTEL_SYSFS_MEM_HEALTH_H
#define INTEL_SYSFS_MEM_HEALTH_H

struct drm_i915_private;
struct intel_gt;
struct kobject;

void intel_mem_health_report_sysfs(struct drm_i915_private *i915);
void intel_gt_sysfs_register_mem(struct intel_gt *gt, struct kobject *parent);

#endif /* INTEL_SYSFS_MEM_HEALTH_H */
