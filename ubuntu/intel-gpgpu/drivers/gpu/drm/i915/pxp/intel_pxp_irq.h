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

#ifndef __INTEL_PXP_IRQ_H__
#define __INTEL_PXP_IRQ_H__

#include <linux/types.h>

struct intel_pxp;

#define GEN12_DISPLAY_PXP_STATE_TERMINATED_INTERRUPT BIT(1)
#define GEN12_DISPLAY_APP_TERMINATED_PER_FW_REQ_INTERRUPT BIT(2)
#define GEN12_DISPLAY_STATE_RESET_COMPLETE_INTERRUPT BIT(3)

#define GEN12_PXP_INTERRUPTS \
	(GEN12_DISPLAY_PXP_STATE_TERMINATED_INTERRUPT | \
	 GEN12_DISPLAY_APP_TERMINATED_PER_FW_REQ_INTERRUPT | \
	 GEN12_DISPLAY_STATE_RESET_COMPLETE_INTERRUPT)

#ifdef CPTCFG_DRM_I915_PXP
void intel_pxp_irq_enable(struct intel_pxp *pxp);
void intel_pxp_irq_disable(struct intel_pxp *pxp);
void intel_pxp_irq_handler(struct intel_pxp *pxp, u16 iir);
#else
static inline void intel_pxp_irq_handler(struct intel_pxp *pxp, u16 iir)
{
}

static inline void intel_pxp_irq_enable(struct intel_pxp *pxp)
{
}

static inline void intel_pxp_irq_disable(struct intel_pxp *pxp)
{
}
#endif

#endif /* __INTEL_PXP_IRQ_H__ */
