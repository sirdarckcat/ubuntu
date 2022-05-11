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
#ifndef _BACKPORTS_LINUX_AF_ARP_H
#define _BACKPORTS_LINUX_AF_ARP_H 1

#include_next <linux/if_arp.h>

#ifndef ARPHRD_IEEE802154_MONITOR
#define ARPHRD_IEEE802154_MONITOR 805	/* IEEE 802.15.4 network monitor */
#endif

#ifndef ARPHRD_6LOWPAN
#define ARPHRD_6LOWPAN	825		/* IPv6 over LoWPAN             */
#endif

#endif /* _BACKPORTS_LINUX_AF_ARP_H */
