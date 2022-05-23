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
 * Copyright © 2014-2019 Intel Corporation
 */

#ifndef _INTEL_GUC_SUBMISSION_H_
#define _INTEL_GUC_SUBMISSION_H_

#include <linux/types.h>

#include "intel_guc.h"

struct drm_printer;
struct intel_engine_cs;

void intel_guc_submission_init_early(struct intel_guc *guc);
int intel_guc_submission_limit_ids(struct intel_guc *guc, u32 limit);
int intel_guc_submission_init(struct intel_guc *guc);
void intel_guc_submission_enable(struct intel_guc *guc);
void intel_guc_submission_disable(struct intel_guc *guc);
void intel_guc_submission_fini(struct intel_guc *guc);
int intel_guc_preempt_work_create(struct intel_guc *guc);
void intel_guc_preempt_work_destroy(struct intel_guc *guc);
int intel_guc_submission_setup(struct intel_engine_cs *engine);
void intel_guc_submission_print_info(struct intel_guc *guc,
				     struct drm_printer *p);
void intel_guc_submission_print_context_info(struct intel_guc *guc,
					     struct drm_printer *p);
void intel_guc_dump_active_requests(struct intel_engine_cs *engine,
				    struct i915_request *hung_rq,
				    struct drm_printer *m);
void intel_guc_busyness_park(struct intel_gt *gt);
void intel_guc_busyness_unpark(struct intel_gt *gt);

bool intel_guc_virtual_engine_has_heartbeat(const struct intel_engine_cs *ve);

int intel_guc_wait_for_pending_msg(struct intel_guc *guc,
				   atomic_t *wait_var,
				   bool interruptible,
				   long timeout);

void intel_guc_context_set_preemption_timeout(struct intel_context *ce,
					      u32 preemption_timeout);

static inline bool intel_guc_submission_is_supported(const struct intel_guc *guc)
{
	return guc->submission_supported;
}

static inline bool intel_guc_submission_is_wanted(const struct intel_guc *guc)
{
	return guc->submission_selected;
}

static inline bool intel_guc_submission_is_used(const struct intel_guc *guc)
{
	return intel_guc_is_used(guc) && intel_guc_submission_is_wanted(guc);
}

int intel_guc_modify_scheduling(struct intel_guc *guc, bool enable);

static inline int
intel_guc_modify_scheduling_start(struct intel_guc *guc, bool enable)
{
	return intel_guc_modify_scheduling(guc, enable);
}

static inline int
intel_guc_modify_scheduling_wait(struct intel_guc *guc)
{

	/*
	 * Even though intel_guc_wait_for_pending_msg can return non-zero, in
	 * practice it never will. Either outstanding_submission_g2h will go to zero and
	 * it return 0 or the heartbeat will kick in and trigger a full GPU
	 * reset. In that case intel_guc_submission_reset_finish is called
	 * which clears outstanding_submission_g2h and wakes this thread.
	 */
	return intel_guc_wait_for_pending_msg(guc,
					      &guc->outstanding_submission_g2h,
					      true,
					      MAX_SCHEDULE_TIMEOUT);

}

#endif
