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
#ifndef _BP_OF_NET_H
#define _BP_OF_NET_H
#include_next <linux/of_net.h>
#include <linux/version.h>

#ifndef CONFIG_OF
#if LINUX_VERSION_IS_LESS(3,10,0)
static inline const void *of_get_mac_address(struct device_node *np)
{
	return NULL;
}
#endif
#endif

/* The behavior of of_get_mac_address() changed in kernel 5.2, it now
 * returns an error code and not NULL in case of an error.
 */
#if LINUX_VERSION_IS_LESS(5,2,0)
static inline const void *backport_of_get_mac_address(struct device_node *np)
{
	const void *mac = of_get_mac_address(np);

	if (!mac)
		return ERR_PTR(-ENODEV);

	return mac;
}
#define of_get_mac_address LINUX_I915_BACKPORT(of_get_mac_address)
#endif /* < 5.2 */

#endif /* _BP_OF_NET_H */
