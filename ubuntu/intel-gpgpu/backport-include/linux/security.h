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
#ifndef __BACKPORT_LINUX_SECURITY_H
#define __BACKPORT_LINUX_SECURITY_H
#include_next <linux/security.h>

#if LINUX_VERSION_IS_LESS(3,1,0)
/*
 * This has been defined in include/linux/security.h for some time, but was
 * only given an EXPORT_SYMBOL for 3.1.  Add a compat_* definition to avoid
 * breaking the compile.
 */
#define security_sk_clone(a, b) compat_security_sk_clone(a, b)

static inline void security_sk_clone(const struct sock *sk, struct sock *newsk)
{
}
#endif

#endif /* __BACKPORT_LINUX_SECURITY_H */
