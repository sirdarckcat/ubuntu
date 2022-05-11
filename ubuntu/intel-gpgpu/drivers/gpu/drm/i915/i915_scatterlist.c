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

bool i915_sg_trim(struct sg_table *orig_st)
{
	struct sg_table new_st;
	struct scatterlist *sg, *new_sg;
	unsigned int i;

	if (orig_st->nents == orig_st->orig_nents)
		return false;

	if (sg_alloc_table(&new_st, orig_st->nents, GFP_KERNEL | __GFP_NOWARN))
		return false;

	new_sg = new_st.sgl;
	for_each_sg(orig_st->sgl, sg, orig_st->nents, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, 0);
		sg_dma_address(new_sg) = sg_dma_address(sg);
		sg_dma_len(new_sg) = sg_dma_len(sg);

		new_sg = sg_next(new_sg);
	}
	GEM_BUG_ON(new_sg); /* Should walk exactly nents and hit the end */

	sg_free_table(orig_st);

	*orig_st = new_st;
	return true;
}

#if IS_ENABLED(CPTCFG_DRM_I915_SELFTEST)
#include "selftests/scatterlist.c"
#endif
