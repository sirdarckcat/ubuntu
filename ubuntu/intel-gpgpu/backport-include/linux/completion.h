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
#ifndef __BACKPORT_COMPLETION_H
#define __BACKPORT_COMPLETION_H
#include_next <linux/completion.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,13,0)
/**
 * reinit_completion - reinitialize a completion structure
 * @x:  pointer to completion structure that is to be reinitialized
 *
 * This inline function should be used to reinitialize a completion structure so it can
 * be reused. This is especially important after complete_all() is used.
 */
#define reinit_completion LINUX_I915_BACKPORT(reinit_completion)
static inline void reinit_completion(struct completion *x)
{
	x->done = 0;
}
#endif /* LINUX_VERSION_IS_LESS(3,13,0) */

#endif /* __BACKPORT_COMPLETION_H */
