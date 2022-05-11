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
/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright © 2018 Intel Corporation
 */

#ifndef __I915_SELFTESTS_IGT_SPINNER_H__
#define __I915_SELFTESTS_IGT_SPINNER_H__

#include "gem/i915_gem_context.h"
#include "gt/intel_engine.h"

#include "i915_drv.h"
#include "i915_request.h"
#include "i915_selftest.h"

struct intel_gt;

struct igt_spinner {
	struct intel_gt *gt;
	struct drm_i915_gem_object *hws;
	struct drm_i915_gem_object *obj;
	struct intel_context *ce;
	struct i915_vma *hws_vma, *batch_vma;
	u32 *batch;
	void *seqno;
};

int igt_spinner_init(struct igt_spinner *spin, struct intel_gt *gt);
int igt_spinner_pin(struct igt_spinner *spin,
		    struct intel_context *ce,
		    struct i915_gem_ww_ctx *ww);
void igt_spinner_fini(struct igt_spinner *spin);

struct i915_request *
igt_spinner_create_request(struct igt_spinner *spin,
			   struct intel_context *ce,
			   u32 arbitration_command);
void igt_spinner_end(struct igt_spinner *spin);

bool igt_wait_for_spinner(struct igt_spinner *spin, struct i915_request *rq);

#endif
