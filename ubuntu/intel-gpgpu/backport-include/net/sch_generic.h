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
#ifndef __BACKPORT_NET_SCH_GENERIC_H
#define __BACKPORT_NET_SCH_GENERIC_H
#include_next <net/sch_generic.h>

#if LINUX_VERSION_IS_LESS(3,3,0)
#if !((LINUX_VERSION_IS_GEQ(3,2,9) && LINUX_VERSION_IS_LESS(3,3,0)) || (LINUX_VERSION_IS_GEQ(3,0,23) && LINUX_VERSION_IS_LESS(3,1,0)))
/* mask qdisc_cb_private_validate as RHEL6 backports this */
#define qdisc_cb_private_validate(a,b) compat_qdisc_cb_private_validate(a,b)
static inline void qdisc_cb_private_validate(const struct sk_buff *skb, int sz)
{
	BUILD_BUG_ON(sizeof(skb->cb) < sizeof(struct qdisc_skb_cb) + sz);
}
#endif
#endif /* LINUX_VERSION_IS_LESS(3,3,0) */

#ifndef TCQ_F_CAN_BYPASS
#define TCQ_F_CAN_BYPASS        4
#endif

#endif /* __BACKPORT_NET_SCH_GENERIC_H */
