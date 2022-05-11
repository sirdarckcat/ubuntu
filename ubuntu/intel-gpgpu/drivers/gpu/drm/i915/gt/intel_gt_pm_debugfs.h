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

#ifndef INTEL_GT_PM_DEBUGFS_H
#define INTEL_GT_PM_DEBUGFS_H

struct intel_gt;
struct dentry;
struct drm_printer;

void intel_gt_pm_debugfs_register(struct intel_gt *gt, struct dentry *root);
void intel_gt_pm_frequency_dump(struct intel_gt *gt, struct drm_printer *m);

/* functions that need to be accessed by the upper level non-gt interfaces */
void intel_gt_pm_debugfs_forcewake_user_open(struct intel_gt *gt);
void intel_gt_pm_debugfs_forcewake_user_release(struct intel_gt *gt);

#endif /* INTEL_GT_PM_DEBUGFS_H */
