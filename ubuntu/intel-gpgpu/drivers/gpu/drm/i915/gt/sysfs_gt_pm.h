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
// SPDX-License-Identifier: MIT
/*
 * Copyright © 2020 Intel Corporation
 */

#ifndef __SYSFS_GT_PM_H__
#define __SYSFS_GT_PM_H__

#include <linux/kobject.h>

#include "intel_gt_types.h"

void intel_gt_sysfs_pm_init(struct intel_gt *gt, struct kobject *kobj);
void intel_gt_sysfs_pm_remove(struct intel_gt *gt, struct kobject *kobj);

#endif /* SYSFS_RC6_H */
