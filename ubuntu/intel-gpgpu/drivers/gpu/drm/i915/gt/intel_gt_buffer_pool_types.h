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

#ifndef INTEL_GT_BUFFER_POOL_TYPES_H
#define INTEL_GT_BUFFER_POOL_TYPES_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "gem/i915_gem_object_types.h"
#include "i915_active_types.h"

struct intel_gt_buffer_pool {
	spinlock_t lock;
	struct list_head cache_list[4];
	struct delayed_work work;
};

struct intel_gt_buffer_pool_node {
	struct i915_active active;
	struct drm_i915_gem_object *obj;
	struct list_head link;
	union {
		struct intel_gt_buffer_pool *pool;
		struct intel_gt_buffer_pool_node *free;
		struct rcu_head rcu;
	};
	unsigned long age;
	enum i915_map_type type;
	u32 pinned;
};

#endif /* INTEL_GT_BUFFER_POOL_TYPES_H */
