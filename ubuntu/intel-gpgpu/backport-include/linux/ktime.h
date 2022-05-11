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
#ifndef __BACKPORT_LINUX_KTIME_H
#define __BACKPORT_LINUX_KTIME_H
#include_next <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/version.h>

#if  LINUX_VERSION_IS_LESS(3,17,0)
#define ktime_get_raw LINUX_I915_BACKPORT(ktime_get_raw)
extern ktime_t ktime_get_raw(void);

#endif /* < 3.17 */

#ifndef ktime_to_timespec64
/* Map the ktime_t to timespec conversion to ns_to_timespec function */
#define ktime_to_timespec64(kt)		ns_to_timespec64((kt).tv64)
#endif

#endif /* __BACKPORT_LINUX_KTIME_H */
