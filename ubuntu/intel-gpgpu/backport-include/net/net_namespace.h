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
#ifndef _COMPAT_NET_NET_NAMESPACE_H
#define _COMPAT_NET_NET_NAMESPACE_H 1

#include_next <net/net_namespace.h>

#if LINUX_VERSION_IS_LESS(3,20,0)
/*
 * In older kernels we simply fail this function.
 */
#define get_net_ns_by_fd	LINUX_I915_BACKPORT(get_net_ns_by_fd)
static inline struct net *get_net_ns_by_fd(int fd)
{
	return ERR_PTR(-EINVAL);
}
#endif

#if LINUX_VERSION_IS_LESS(4,1,0)
typedef struct {
#ifdef CONFIG_NET_NS
	struct net *net;
#endif
} possible_net_t;

static inline void possible_write_pnet(possible_net_t *pnet, struct net *net)
{
#ifdef CONFIG_NET_NS
	pnet->net = net;
#endif
}

static inline struct net *possible_read_pnet(const possible_net_t *pnet)
{
#ifdef CONFIG_NET_NS
	return pnet->net;
#else
	return &init_net;
#endif
}
#else
#define possible_write_pnet(pnet, net) write_pnet(pnet, net)
#define possible_read_pnet(pnet) read_pnet(pnet)
#endif /* LINUX_VERSION_IS_LESS(4,1,0) */

#endif	/* _COMPAT_NET_NET_NAMESPACE_H */
