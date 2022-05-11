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
#ifndef __BACKPORT_LINUX_OF_PLATFORM_H
#define __BACKPORT_LINUX_OF_PLATFORM_H
#include_next <linux/of_platform.h>
#include <linux/version.h>
#include <linux/of.h>
/* upstream now includes this here and some people rely on it */
#include <linux/of_device.h>

#if LINUX_VERSION_IS_LESS(3,4,0) && !defined(CONFIG_OF_DEVICE)
struct of_dev_auxdata;
#define of_platform_populate LINUX_I915_BACKPORT(of_platform_populate)
static inline int of_platform_populate(struct device_node *root,
					const struct of_device_id *matches,
					const struct of_dev_auxdata *lookup,
					struct device *parent)
{
	return -ENODEV;
}
#endif /* LINUX_VERSION_IS_LESS(3,4,0) */

#if LINUX_VERSION_IS_LESS(3,11,0) && !defined(CONFIG_OF_DEVICE)
extern const struct of_device_id of_default_bus_match_table[];
#endif /* LINUX_VERSION_IS_LESS(3,11,0) */

#if LINUX_VERSION_IS_LESS(4,3,0) && !defined(CONFIG_OF_DEVICE)
struct of_dev_auxdata;
#define of_platform_default_populate \
	LINUX_I915_BACKPORT(of_platform_default_populate)
static inline int
of_platform_default_populate(struct device_node *root,
			     const struct of_dev_auxdata *lookup,
			     struct device *parent)
{
	return -ENODEV;
}
#endif /* LINUX_VERSION_IS_LESS(4,3,0) */

#endif /* __BACKPORT_LINUX_OF_PLATFORM_H */
