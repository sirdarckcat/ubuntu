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
 * Copyright © 2016 Intel Corporation
 */

#ifndef __MOCK_ENGINE_H__
#define __MOCK_ENGINE_H__

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/timer.h>

#include "gt/intel_engine.h"

struct mock_engine {
	struct intel_engine_cs base;

	spinlock_t hw_lock;
	struct list_head hw_queue;
	struct timer_list hw_delay;
};

struct intel_engine_cs *mock_engine(struct drm_i915_private *i915,
				    const char *name,
				    int id);
int mock_engine_init(struct intel_engine_cs *engine);

void mock_engine_flush(struct intel_engine_cs *engine);
void mock_engine_reset(struct intel_engine_cs *engine);
void mock_engine_free(struct intel_engine_cs *engine);

#endif /* !__MOCK_ENGINE_H__ */
