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
#ifndef __BACKPORT_LINUX_TIMECOUNTER_H
#define __BACKPORT_LINUX_TIMECOUNTER_H

#if LINUX_VERSION_IS_GEQ(3,20,0)
#include_next <linux/timecounter.h>
#else
#include <linux/clocksource.h>

/**
 * timecounter_adjtime - Shifts the time of the clock.
 * @delta:	Desired change in nanoseconds.
 */
#define timecounter_adjtime LINUX_I915_BACKPORT(timecounter_adjtime)
static inline void timecounter_adjtime(struct timecounter *tc, s64 delta)
{
	tc->nsec += delta;
}
#endif

#ifndef CYCLECOUNTER_MASK
/* simplify initialization of mask field */
#define CYCLECOUNTER_MASK(bits) (cycle_t)((bits) < 64 ? ((1ULL<<(bits))-1) : -1)
#endif

#endif /* __BACKPORT_LINUX_TIMECOUNTER_H */
