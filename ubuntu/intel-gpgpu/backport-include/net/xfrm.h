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
#ifndef __BACKPORT_NET_XFRM_H
#define __BACKPORT_NET_XFRM_H
#include_next <net/xfrm.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(5,4,0)
#define skb_ext_reset LINUX_I915_BACKPORT(skb_ext_reset)
static inline void skb_ext_reset(struct sk_buff *skb)
{
	secpath_reset(skb);
}
#endif

#endif /* __BACKPORT_NET_XFRM_H */
