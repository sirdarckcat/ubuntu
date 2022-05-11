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
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_H__
#define __INTEL_PXP_H__

#include "gt/intel_gt_types.h"
#include "intel_pxp_types.h"

static inline struct intel_gt *pxp_to_gt(const struct intel_pxp *pxp)
{
	return container_of(pxp, struct intel_gt, pxp);
}

static inline bool intel_pxp_is_enabled(const struct intel_pxp *pxp)
{
	return pxp->ce;
}

static inline bool intel_pxp_is_active(const struct intel_pxp *pxp)
{
	return pxp->arb_is_valid;
}

void intel_pxp_init(struct intel_pxp *pxp);
void intel_pxp_fini(struct intel_pxp *pxp);

void intel_pxp_init_hw(struct intel_pxp *pxp);
void intel_pxp_fini_hw(struct intel_pxp *pxp);

void intel_pxp_mark_termination_in_progress(struct intel_pxp *pxp);
int intel_pxp_wait_for_arb_start(struct intel_pxp *pxp);

#endif /* __INTEL_PXP_H__ */
