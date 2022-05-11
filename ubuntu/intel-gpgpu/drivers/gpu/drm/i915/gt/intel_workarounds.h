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
 * Copyright © 2014-2018 Intel Corporation
 */

#ifndef _INTEL_WORKAROUNDS_H_
#define _INTEL_WORKAROUNDS_H_

#include <linux/slab.h>

#include "intel_workarounds_types.h"

struct drm_i915_private;
struct i915_request;
struct intel_engine_cs;
struct intel_gt;

static inline void intel_wa_list_free(struct i915_wa_list *wal)
{
	kfree(wal->list);
	memset(wal, 0, sizeof(*wal));
}

void intel_engine_init_ctx_wa(struct intel_engine_cs *engine);
int intel_engine_emit_ctx_wa(struct i915_request *rq);

void intel_gt_init_workarounds(struct intel_gt *gt);
void intel_gt_apply_workarounds(struct intel_gt *gt);
bool intel_gt_verify_workarounds(struct intel_gt *gt, const char *from);

void intel_engine_init_whitelist(struct intel_engine_cs *engine);
void intel_engine_apply_whitelist(struct intel_engine_cs *engine);

void intel_engine_init_workarounds(struct intel_engine_cs *engine);
void intel_engine_apply_workarounds(struct intel_engine_cs *engine);
int intel_engine_verify_workarounds(struct intel_engine_cs *engine,
				    const char *from);

void intel_engine_allow_user_register_access(struct intel_engine_cs *engine,
					     struct i915_whitelist_reg *reg,
					     u32 count);
void intel_engine_deny_user_register_access(struct intel_engine_cs *engine,
					    struct i915_whitelist_reg *reg,
					    u32 count);

#endif
