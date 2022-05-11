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
 * Copyright © 2021 Intel Corporation
 */

#ifndef __I915_GEM_EVICT_H__
#define __I915_GEM_EVICT_H__

#include <linux/types.h>

struct drm_mm_node;
struct i915_address_space;

int __must_check i915_gem_evict_something(struct i915_address_space *vm,
					  u64 min_size, u64 alignment,
					  unsigned long color,
					  u64 start, u64 end,
					  unsigned flags);
int __must_check i915_gem_evict_for_node(struct i915_address_space *vm,
					 struct drm_mm_node *node,
					 unsigned int flags);
int i915_gem_evict_vm(struct i915_address_space *vm);

#endif /* __I915_GEM_EVICT_H__ */
