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
 * Copyright (C) 2018 Intel Corporation
 */
#ifndef __BACKPORT_PERCPU_H
#define __BACKPORT_PERCPU_H
#include_next <linux/percpu.h>

#if LINUX_VERSION_IS_LESS(3,18,0)
static inline void __percpu *__alloc_gfp_warn(void)
{
	WARN(1, "Cannot backport alloc_percpu_gfp");
	return NULL;
}

#define alloc_percpu_gfp(type, gfp) \
	({ (gfp == GFP_KERNEL) ? alloc_percpu(type) : __alloc_gfp_warn(); })
#endif /* LINUX_VERSION_IS_LESS(3,18,0) */

#endif /* __BACKPORT_PERCPU_H */
