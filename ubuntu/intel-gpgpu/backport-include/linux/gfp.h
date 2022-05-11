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
#ifndef __BACKPORT_LINUX_GFP_H
#define __BACKPORT_LINUX_GFP_H
#include_next <linux/gfp.h>

#ifndef ___GFP_KSWAPD_RECLAIM
#define ___GFP_KSWAPD_RECLAIM	0x0u
#endif

#ifndef __GFP_KSWAPD_RECLAIM
#define __GFP_KSWAPD_RECLAIM	((__force gfp_t)___GFP_KSWAPD_RECLAIM) /* kswapd can wake */
#endif

#if LINUX_VERSION_IS_LESS(4,10,0) && LINUX_VERSION_IS_GEQ(4,2,0)
#define page_frag_alloc LINUX_I915_BACKPORT(page_frag_alloc)
static inline void *page_frag_alloc(struct page_frag_cache *nc,
				    unsigned int fragsz, gfp_t gfp_mask)
{
	return __alloc_page_frag(nc, fragsz, gfp_mask);
}

#define __page_frag_cache_drain LINUX_I915_BACKPORT(__page_frag_cache_drain)
void __page_frag_cache_drain(struct page *page, unsigned int count);
#endif /* < 4.10 && >= 4.2 */

#endif /* __BACKPORT_LINUX_GFP_H */
