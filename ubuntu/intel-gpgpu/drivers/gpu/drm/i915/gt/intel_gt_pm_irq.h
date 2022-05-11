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

#ifndef INTEL_GT_PM_IRQ_H
#define INTEL_GT_PM_IRQ_H

#include <linux/types.h>

struct intel_gt;

void gen6_gt_pm_unmask_irq(struct intel_gt *gt, u32 mask);
void gen6_gt_pm_mask_irq(struct intel_gt *gt, u32 mask);

void gen6_gt_pm_enable_irq(struct intel_gt *gt, u32 enable_mask);
void gen6_gt_pm_disable_irq(struct intel_gt *gt, u32 disable_mask);

void gen6_gt_pm_reset_iir(struct intel_gt *gt, u32 reset_mask);

#endif /* INTEL_GT_PM_IRQ_H */
