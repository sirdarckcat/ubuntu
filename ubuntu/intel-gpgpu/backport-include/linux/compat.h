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
#ifndef __BACKPORT_COMPAT_H
#define __BACKPORT_COMPAT_H

#include_next <linux/compat.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,4,0)
#ifdef CONFIG_X86_X32_ABI
#define COMPAT_USE_64BIT_TIME \
	(!!(task_pt_regs(current)->orig_ax & __X32_SYSCALL_BIT))
#else
#define COMPAT_USE_64BIT_TIME 0
#endif
#endif

#if LINUX_VERSION_IS_LESS(3,4,0)
#define compat_put_timespec LINUX_I915_BACKPORT(compat_put_timespec)
extern int compat_put_timespec(const struct timespec *, void __user *);
#endif

#endif /* __BACKPORT_COMPAT_H */
