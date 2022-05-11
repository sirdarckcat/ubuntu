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
 * Copyright © 2019 Intel Corporation
 */

#ifndef _I915_GLOBALS_H_
#define _I915_GLOBALS_H_

#include <linux/errno.h>
#include <linux/types.h>

struct drm_printer;
struct kmem_cache;

typedef void (*i915_global_func_t)(void);
typedef void (*i915_global_show_t)(struct drm_printer *p);

struct i915_global {
	struct list_head link;

	i915_global_show_t show;
	i915_global_func_t shrink;
	i915_global_func_t exit;
};

void i915_global_register(struct i915_global *global);

#if IS_ENABLED(CONFIG_SLUB_DEBUG) || IS_ENABLED(CONFIG_SLAB)
int i915_globals_show(struct drm_printer *p);
void i915_globals_show_slab(struct kmem_cache *cache,
			    const char *name,
			    struct drm_printer *p);
#else
static inline int i915_globals_show(struct drm_printer *p) { return -ENODEV;}
static inline void i915_globals_show_slab(struct kmem_cache *cache,
					  const char *name,
					  struct drm_printer *p) {}
#endif

int i915_globals_init(void);
void i915_globals_park(void);
void i915_globals_unpark(void);
void i915_globals_drain(void);
void i915_globals_exit(void);

/* constructors */
int i915_global_active_init(void);
int i915_global_buddy_init(void);
int i915_global_context_init(void);
int i915_global_gem_context_init(void);
int i915_global_objects_init(void);
int i915_global_request_init(void);
int i915_global_scheduler_init(void);
int i915_global_vma_init(void);

#endif /* _I915_GLOBALS_H_ */
