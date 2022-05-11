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
#ifndef __BACKPORT_DMA_RESV_H
#define __BACKPORT_DMA_RESV_H

#include_next <linux/dma-resv.h>

/**
 * dma_resv_get_excl_unlocked - get the reservation object's
 * exclusive fence, without lock held.
 * @obj: the reservation object
 *
 * If there is an exclusive fence, this atomically increments it's
 * reference count and returns it.
 *
 * RETURNS
 * The exclusive fence or NULL if none
 */
#define dma_resv_get_excl_unlocked LINUX_I915_BACKPORT(dma_resv_get_excl_unlocked)
static inline struct dma_fence *
dma_resv_get_excl_unlocked(struct dma_resv *obj)
{
        struct dma_fence *fence;

        if (!rcu_access_pointer(obj->fence_excl))
                return NULL;

        rcu_read_lock();
        fence = dma_fence_get_rcu_safe(&obj->fence_excl);
        rcu_read_unlock();

        return fence;
}

#endif /* __BACKPORT_DMA_RESV_H */
