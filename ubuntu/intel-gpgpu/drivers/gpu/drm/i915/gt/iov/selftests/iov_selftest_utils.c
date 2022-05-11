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
 * Copyright © 2021 Intel Corporation
 */

#include "gt/intel_gtt.h"
#include "gt/iov/intel_iov_utils.h"

#include "iov_selftest_utils.h"

bool intel_iov_check_ggtt_vfid(struct intel_iov *iov, void __iomem *pte_addr, u16 vfid)
{
	GEM_BUG_ON(!HAS_SRIOV(iov_to_i915(iov)));

	if (i915_ggtt_has_xehpsdv_pte_vfid_mask(iov_to_gt(iov)->ggtt))
		return vfid == FIELD_GET(XEHPSDV_GGTT_PTE_VFID_MASK, gen8_get_pte(pte_addr));
	else
		return vfid == FIELD_GET(TGL_GGTT_PTE_VFID_MASK, gen8_get_pte(pte_addr));
}
