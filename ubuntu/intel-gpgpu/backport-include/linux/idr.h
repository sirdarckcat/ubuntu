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
#ifndef __BACKPORT_IDR_H
#define __BACKPORT_IDR_H
/* some versions have a broken idr header */
#include <linux/spinlock.h>
#include_next <linux/idr.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,1,0)
#define ida_simple_get LINUX_I915_BACKPORT(ida_simple_get)
int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,
		   gfp_t gfp_mask);

#define ida_simple_remove LINUX_I915_BACKPORT(ida_simple_remove)
void ida_simple_remove(struct ida *ida, unsigned int id);
#endif

#if LINUX_VERSION_IS_LESS(3,9,0)
#include <linux/errno.h>
/**
 * backport of idr idr_alloc() usage
 *
 * This backports a patch series send by Tejun Heo:
 * https://lkml.org/lkml/2013/2/2/159
 */
static inline void compat_idr_destroy(struct idr *idp)
{
	idr_remove_all(idp);
	idr_destroy(idp);
}
#define idr_destroy(idp) compat_idr_destroy(idp)

static inline int idr_alloc(struct idr *idr, void *ptr, int start, int end,
			    gfp_t gfp_mask)
{
	int id, ret;

	do {
		if (!idr_pre_get(idr, gfp_mask))
			return -ENOMEM;
		ret = idr_get_new_above(idr, ptr, start, &id);
		if (!ret && id > end) {
			idr_remove(idr, id);
			ret = -ENOSPC;
		}
	} while (ret == -EAGAIN);

	return ret ? ret : id;
}

static inline void idr_preload(gfp_t gfp_mask)
{
}

static inline void idr_preload_end(void)
{
}
#endif

#ifndef idr_for_each_entry
#define idr_for_each_entry(idp, entry, id)			\
	for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; ++id)
#endif

#if LINUX_VERSION_IS_LESS(4, 11, 0)
static inline void *backport_idr_remove(struct idr *idr, int id)
{
	void *item = idr_find(idr, id);
	idr_remove(idr, id);
	return item;
}
#define idr_remove	backport_idr_remove
#endif

#endif /* __BACKPORT_IDR_H */
