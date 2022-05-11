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

#ifndef __INTEL_GT_DEBUG_
#define __INTEL_GT_DEBUG_

#include "intel_gt_types.h"

int intel_gt_eu_threads_needing_attention(struct intel_gt *gt);

int intel_gt_for_each_compute_slice_subslice(struct intel_gt *gt,
					     bool write,
					     int (*fn)(struct intel_gt *gt,
						       void *data,
						       unsigned int slice,
						       unsigned int subslice,
						       bool subslice_present),
					     void *data);
#endif
