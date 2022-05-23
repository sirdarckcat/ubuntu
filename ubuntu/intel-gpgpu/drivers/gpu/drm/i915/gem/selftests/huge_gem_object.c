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
 * Copyright © 2016 Intel Corporation
 */

#include "i915_scatterlist.h"

#include "huge_gem_object.h"

static void huge_free_pages(struct drm_i915_gem_object *obj,
			    struct sg_table *pages)
{
	unsigned long nreal = obj->scratch / PAGE_SIZE;
	struct sgt_iter sgt_iter;
	struct page *page;

	for_each_sgt_page(page, sgt_iter, pages) {
		__free_page(page);
		if (!--nreal)
			break;
	}

	sg_free_table(pages);
	kfree(pages);
}

static int huge_get_pages(struct drm_i915_gem_object *obj)
{
#define GFP (GFP_KERNEL | __GFP_NOWARN | __GFP_RETRY_MAYFAIL)
	const unsigned long nreal = obj->scratch / PAGE_SIZE;
	const unsigned long npages = obj->base.size / PAGE_SIZE;
	struct scatterlist *sg, *src, *end;
	struct sg_table *pages;
	unsigned long n;

	pages = kmalloc(sizeof(*pages), GFP);
	if (!pages)
		return -ENOMEM;

	if (sg_alloc_table(pages, npages, GFP)) {
		kfree(pages);
		return -ENOMEM;
	}

	sg = pages->sgl;
	for (n = 0; n < nreal; n++) {
		struct page *page;

		page = alloc_page(GFP | __GFP_HIGHMEM);
		if (!page) {
			sg_mark_end(sg);
			goto err;
		}

		sg_set_page(sg, page, PAGE_SIZE, 0);
		sg = __sg_next(sg);
	}
	if (nreal < npages) {
		for (end = sg, src = pages->sgl; sg; sg = __sg_next(sg)) {
			sg_set_page(sg, sg_page(src), PAGE_SIZE, 0);
			src = __sg_next(src);
			if (src == end)
				src = pages->sgl;
		}
	}

	if (i915_gem_gtt_prepare_pages(obj, pages))
		goto err;

	__i915_gem_object_set_pages(obj, pages, PAGE_SIZE);

	return 0;

err:
	huge_free_pages(obj, pages);
	return -ENOMEM;
#undef GFP
}

static int huge_put_pages(struct drm_i915_gem_object *obj,
			   struct sg_table *pages)
{
	i915_gem_gtt_finish_pages(obj, pages);
	huge_free_pages(obj, pages);

	obj->mm.dirty = false;

	return 0;
}

static const struct drm_i915_gem_object_ops huge_ops = {
	.name = "huge-gem",
	.get_pages = huge_get_pages,
	.put_pages = huge_put_pages,
};

struct drm_i915_gem_object *
huge_gem_object(struct drm_i915_private *i915,
		phys_addr_t phys_size,
		dma_addr_t dma_size)
{
	static struct lock_class_key lock_class;
	struct drm_i915_gem_object *obj;
	unsigned int cache_level;

	GEM_BUG_ON(!phys_size || phys_size > dma_size);
	GEM_BUG_ON(!IS_ALIGNED(phys_size, PAGE_SIZE));
	GEM_BUG_ON(!IS_ALIGNED(dma_size, I915_GTT_PAGE_SIZE));

	if (overflows_type(dma_size, obj->base.size))
		return ERR_PTR(-E2BIG);

	obj = i915_gem_object_alloc();
	if (!obj)
		return ERR_PTR(-ENOMEM);

	drm_gem_private_object_init(&i915->drm, &obj->base, dma_size);
	i915_gem_object_init(obj, &huge_ops, &lock_class,
			     I915_BO_ALLOC_STRUCT_PAGE);

	obj->read_domains = I915_GEM_DOMAIN_CPU;
	obj->write_domain = I915_GEM_DOMAIN_CPU;
	cache_level = HAS_LLC(i915) ? I915_CACHE_LLC : I915_CACHE_NONE;
	i915_gem_object_set_cache_coherency(obj, cache_level);
	obj->scratch = phys_size;

	return obj;
}
