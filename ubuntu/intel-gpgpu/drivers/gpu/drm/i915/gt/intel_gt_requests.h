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

#ifndef INTEL_GT_REQUESTS_H
#define INTEL_GT_REQUESTS_H

#include <stddef.h>

struct intel_engine_cs;
struct intel_gt;
struct intel_timeline;

long intel_gt_retire_requests_timeout(struct intel_gt *gt, long timeout,
				      long *remaining_timeout);
static inline void intel_gt_retire_requests(struct intel_gt *gt)
{
	intel_gt_retire_requests_timeout(gt, 0, NULL);
}

void intel_engine_init_retire(struct intel_engine_cs *engine);
void intel_engine_add_retire(struct intel_engine_cs *engine,
			     struct intel_timeline *tl);
void intel_engine_fini_retire(struct intel_engine_cs *engine);

void intel_gt_init_requests(struct intel_gt *gt);
void intel_gt_park_requests(struct intel_gt *gt);
void intel_gt_unpark_requests(struct intel_gt *gt);
void intel_gt_fini_requests(struct intel_gt *gt);

#endif /* INTEL_GT_REQUESTS_H */
