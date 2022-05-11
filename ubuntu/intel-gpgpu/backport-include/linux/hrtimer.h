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
#ifndef __BACKPORT_LINUX_HRTIMER_H
#define __BACKPORT_LINUX_HRTIMER_H
#include <linux/version.h>
#include_next <linux/hrtimer.h>

#if LINUX_VERSION_IS_LESS(4,16,0)

#define HRTIMER_MODE_ABS_SOFT HRTIMER_MODE_ABS
#define HRTIMER_MODE_REL_SOFT HRTIMER_MODE_REL

#endif /* < 4.16 */

#endif /* __BACKPORT_LINUX_HRTIMER_H */
