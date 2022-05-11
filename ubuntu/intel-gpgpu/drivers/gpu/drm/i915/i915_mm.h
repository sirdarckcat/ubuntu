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

#ifndef __I915_MM_H__
#define __I915_MM_H__

#include <linux/bug.h>
#include <linux/types.h>

struct vm_area_struct;
struct io_mapping;
struct scatterlist;

#if IS_ENABLED(CONFIG_X86)
int remap_io_mapping(struct vm_area_struct *vma,
		     unsigned long addr, unsigned long pfn, unsigned long size,
		     struct io_mapping *iomap);
#else
static inline
int remap_io_mapping(struct vm_area_struct *vma,
		     unsigned long addr, unsigned long pfn, unsigned long size,
		     struct io_mapping *iomap)
{
	WARN_ONCE(1, "Architecture has no drm_cache.c support\n");
	return 0;
}
#endif

int remap_io_sg(struct vm_area_struct *vma,
		unsigned long addr, unsigned long size,
		struct scatterlist *sgl, resource_size_t iobase);

#endif /* __I915_MM_H__ */
