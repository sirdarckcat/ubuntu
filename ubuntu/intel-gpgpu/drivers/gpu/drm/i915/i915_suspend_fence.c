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
// SPDX-License-Identifier: MIT
/*
 * Copyright © 2020 Intel Corporation
 */

#include "gt/intel_context.h"

#include "i915_suspend_fence.h"

static void suspend_fence_release(struct dma_fence_work *f)
{
	struct i915_suspend_fence *sfence =
		container_of(f, typeof(*sfence), base);

	if (sfence->block_requested) {
		if (intel_context_is_active(sfence->ce))
			sfence->ops->revalidate(sfence->ce);
		intel_context_resume(sfence->ce);
	}
	intel_context_put(sfence->ce);
	sfence->ce = NULL;
}

static int suspend_fence_work(struct dma_fence_work *f)
{
	return 0;
}

static void suspend_fence_suspend(struct i915_suspend_fence *sfence,
				  bool atomic)
{
	if (intel_context_is_active(sfence->ce)) {
		struct i915_sw_fence *block_completed =
			intel_context_suspend(sfence->ce, atomic);
		int ret;

		/*
		 * If we're in atomic context, we might see -EBUSY,
		 * then queue a retry.
		 */
		if (IS_ERR(block_completed)) {
			GEM_BUG_ON(!atomic || PTR_ERR(block_completed) != -EBUSY);
			queue_work(sfence->base.wq, &sfence->suspend_work);
			return;
		}

		ret = i915_sw_fence_await_sw_fence(&sfence->base.chain,
						   block_completed,
						   &sfence->block_wq);
		GEM_BUG_ON(ret < 0);
		sfence->block_requested = true;
	}

	i915_sw_fence_complete(&sfence->base.chain);
}

static void suspend_fence_suspend_work(struct work_struct *work)
{
	struct i915_suspend_fence *sfence =
		container_of(work, typeof(*sfence), suspend_work);

	suspend_fence_suspend(sfence, false);
}

static bool suspend_fence_enable_signaling(struct dma_fence_work *f)
{
	struct i915_suspend_fence *sfence =
		container_of(f, typeof(*sfence), base);

	suspend_fence_suspend(sfence, true);

	return true;
}

static const struct dma_fence_work_ops suspend_fence_ops = {
	.name = "suspend_fence",
	.work = suspend_fence_work,
	.release = suspend_fence_release,
	.enable_signaling = suspend_fence_enable_signaling,
};

/**
 * i915_suspend_fence_init - Initialize a suspend fence
 * @sfence: The suspend fence to initialize.
 * @ce: The context
 * @ops: The struct suspend_fence_ops structure
 *
 * Return: A refcounted struct dma_fence pointer to the embedded
 * struct dma_fence. No error pointers. Note that the refcount
 * cannot be transferred - it will be eventually released after
 * dma_fence_enable_sw_signaling() is called on the embedded
 * struct dma_fence.
 */
struct dma_fence *
i915_suspend_fence_init(struct i915_suspend_fence *sfence,
			struct intel_context *ce,
			const struct i915_suspend_fence_ops *ops)
{
	struct dma_fence_work *base = &sfence->base;
	int ret;

	/* Make sure fence_free works. */
	BUILD_BUG_ON(offsetof(struct i915_suspend_fence, base.dma) != 0);

	lockdep_assert_held(&ce->timeline->mutex);
	INIT_WORK(&sfence->suspend_work, suspend_fence_suspend_work);
	dma_fence_work_init(base, NULL, &suspend_fence_ops);
	__set_bit(I915_SUSPEND_FENCE, &base->dma.flags);
	sfence->ops = ops;

	sfence->ce = intel_context_get(ce);
	ret = i915_sw_fence_await(&sfence->base.chain);
	GEM_BUG_ON(!ret);

	dma_fence_work_commit(base);

	return &base->dma;
}

/**
 * i915_suspend_fence_retire_dma_fence - Retire the suspend fence
 * @fence the struct dma_fence embedded in a i915_suspend_fence
 *
 * This function signals the suspend fence and releases references
 * necessary for it to signal, leaving only external references.
 */
void i915_suspend_fence_retire_dma_fence(struct dma_fence *fence)
{
	/*
	 * Need to call enable_sw_signaling to make the dma_fence_work
	 * signal and release its own reference.
	 */
	dma_fence_enable_sw_signaling(fence);
	dma_fence_put(fence);
}
