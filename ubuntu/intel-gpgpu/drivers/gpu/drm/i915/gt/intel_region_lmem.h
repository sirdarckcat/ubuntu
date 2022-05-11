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

#ifndef __INTEL_REGION_LMEM_H
#define __INTEL_REGION_LMEM_H

#include <linux/types.h>

struct intel_gt;

struct intel_memory_region *intel_gt_setup_lmem(struct intel_gt *gt);
int intel_get_tile_range(struct intel_gt *gt,
			 resource_size_t *lmem_base,
			 resource_size_t *lmem_size);

#endif /* !__INTEL_REGION_LMEM_H */
