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

#ifndef INTEL_RING_TYPES_H
#define INTEL_RING_TYPES_H

#include <linux/atomic.h>
#include <linux/kref.h>
#include <linux/types.h>

/*
 * Early gen2 devices have a cacheline of just 32 bytes, using 64 is overkill,
 * but keeps the logic simple. Indeed, the whole purpose of this macro is just
 * to give some inclination as to some of the magic values used in the various
 * workarounds!
 */
#define CACHELINE_BYTES 64
#define CACHELINE_DWORDS (CACHELINE_BYTES / sizeof(u32))

struct i915_vma;

struct intel_ring {
	struct kref ref;
	struct i915_vma *vma;
	void *vaddr;

	/*
	 * As we have two types of rings, one global to the engine used
	 * by ringbuffer submission and those that are exclusive to a
	 * context used by execlists, we have to play safe and allow
	 * atomic updates to the pin_count. However, the actual pinning
	 * of the context is either done during initialisation for
	 * ringbuffer submission or serialised as part of the context
	 * pinning for execlists, and so we do not need a mutex ourselves
	 * to serialise intel_ring_pin/intel_ring_unpin.
	 */
	atomic_t pin_count;

	u32 head; /* updated during retire, loosely tracks RING_HEAD */
	u32 tail; /* updated on submission, used for RING_TAIL */
	u32 emit; /* updated during request construction */

	u32 space;
	u32 size;
	u32 wrap;
	u32 effective_size;
};

#endif /* INTEL_RING_TYPES_H */
