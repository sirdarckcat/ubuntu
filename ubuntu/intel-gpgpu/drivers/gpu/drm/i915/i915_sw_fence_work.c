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
 * Copyright © 2019 Intel Corporation
 */

#include "i915_sw_fence_work.h"

static void fence_work(struct work_struct *work)
{
	struct dma_fence_work *f = container_of(work, typeof(*f), work);
	bool cookie = dma_fence_begin_signalling();

	if (!f->dma.error) {
		int err;

		err = f->ops->work(f);
		if (err)
			dma_fence_set_error(&f->dma, err);
	}

	dma_fence_signal(&f->dma);
	dma_fence_end_signalling(cookie);

	if (f->ops->release)
		f->ops->release(f);

	dma_fence_put(&f->dma);
}

static int
fence_notify(struct i915_sw_fence *fence, enum i915_sw_fence_notify state)
{
	struct dma_fence_work *f = container_of(fence, typeof(*f), chain);

	switch (state) {
	case FENCE_COMPLETE:
		if (fence->error)
			dma_fence_set_error(&f->dma, fence->error);

		dma_fence_get(&f->dma);
		if (test_bit(DMA_FENCE_WORK_IMM, &f->dma.flags))
			fence_work(&f->work);
		else
			queue_work(f->wq, &f->work);
		break;

	case FENCE_FREE:
		dma_fence_put(&f->dma);
		break;
	}

	return NOTIFY_DONE;
}

static const char *get_driver_name(struct dma_fence *fence)
{
	return "dma-fence";
}

static const char *get_timeline_name(struct dma_fence *fence)
{
	struct dma_fence_work *f = container_of(fence, typeof(*f), dma);

	return f->ops->name ?: "work";
}

static void fence_release(struct dma_fence *fence)
{
	struct dma_fence_work *f = container_of(fence, typeof(*f), dma);

	i915_sw_fence_fini(&f->chain);

	BUILD_BUG_ON(offsetof(typeof(*f), dma));
	dma_fence_free(&f->dma);
}

static bool fence_enable_signaling(struct dma_fence *fence)
{
	struct dma_fence_work *f = container_of(fence, typeof(*f), dma);

	if (f->ops->enable_signaling)
		return f->ops->enable_signaling(f);

	return true;
}

const struct dma_fence_ops sw_fence_work_ops = {
	.get_driver_name = get_driver_name,
	.get_timeline_name = get_timeline_name,
	.enable_signaling = fence_enable_signaling,
	.release = fence_release,
};

void dma_fence_work_init(struct dma_fence_work *f,
			 struct workqueue_struct *wq,
			 const struct dma_fence_work_ops *ops)
{
	f->ops = ops;
	f->wq = wq ? : system_unbound_wq;
	spin_lock_init(&f->lock);
	dma_fence_init(&f->dma, &sw_fence_work_ops, &f->lock, 0, 0);
	i915_sw_fence_init(&f->chain, fence_notify);
	INIT_WORK(&f->work, fence_work);
}

int dma_fence_work_chain(struct dma_fence_work *f, struct dma_fence *signal)
{
	if (!signal)
		return 0;

	return __i915_sw_fence_await_dma_fence(&f->chain, signal, &f->cb);
}
