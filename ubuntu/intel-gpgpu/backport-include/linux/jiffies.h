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
#ifndef __BACKPORT_LNIUX_JIFFIES_H
#define __BACKPORT_LNIUX_JIFFIES_H
#include_next <linux/jiffies.h>

#ifndef time_is_before_jiffies
#define time_is_before_jiffies(a) time_after(jiffies, a)
#endif

#ifndef time_is_after_jiffies
#define time_is_after_jiffies(a) time_before(jiffies, a)
#endif

#ifndef time_is_before_eq_jiffies
#define time_is_before_eq_jiffies(a) time_after_eq(jiffies, a)
#endif

#ifndef time_is_after_eq_jiffies
#define time_is_after_eq_jiffies(a) time_before_eq(jiffies, a)
#endif

/*
 * This function is available, but not exported in kernel < 3.17, add
 * an own version.
 */
#if LINUX_VERSION_IS_LESS(3,17,0)
#define nsecs_to_jiffies LINUX_I915_BACKPORT(nsecs_to_jiffies)
extern unsigned long nsecs_to_jiffies(u64 n);
#endif /* 3.17 */

#endif /* __BACKPORT_LNIUX_JIFFIES_H */
