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
#ifndef _BACKPORT_NET_ADDRCONF_H
#define _BACKPORT_NET_ADDRCONF_H 1

#include_next <net/addrconf.h>

#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,9,0)
static inline bool ipv6_addr_is_solict_mult(const struct in6_addr *addr)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) && BITS_PER_LONG == 64
	__u64 *p = (__u64 *)addr;
	return ((p[0] ^ cpu_to_be64(0xff02000000000000UL)) |
		((p[1] ^ cpu_to_be64(0x00000001ff000000UL)) &
		 cpu_to_be64(0xffffffffff000000UL))) == 0UL;
#else
	return ((addr->s6_addr32[0] ^ htonl(0xff020000)) |
		addr->s6_addr32[1] |
		(addr->s6_addr32[2] ^ htonl(0x00000001)) |
		(addr->s6_addr[12] ^ 0xff)) == 0;
#endif
}
#endif /* LINUX_VERSION_IS_LESS(3,9,0) */

#endif	/* _BACKPORT_NET_ADDRCONF_H */
