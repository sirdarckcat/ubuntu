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
#ifndef __BACKPORT_LINUX_POLL_H
#define __BACKPORT_LINUX_POLL_H
#include_next <linux/poll.h>
#include <linux/version.h>
#include <linux/eventpoll.h>

#if  LINUX_VERSION_IS_LESS(3,4,0)
#define poll_does_not_wait LINUX_I915_BACKPORT(poll_does_not_wait)
static inline bool poll_does_not_wait(const poll_table *p)
{
	return p == NULL || p->qproc == NULL;
}

#define poll_requested_events LINUX_I915_BACKPORT(poll_requested_events)
static inline unsigned long poll_requested_events(const poll_table *p)
{
	return p ? p->key : ~0UL;
}
#endif /* < 3.4 */

#endif /* __BACKPORT_LINUX_POLL_H */
