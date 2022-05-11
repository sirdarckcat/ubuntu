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
#ifndef __BACKPORT_LINUX_PROPERTY_H_
#define __BACKPORT_LINUX_PROPERTY_H_
#include <linux/version.h>
#if LINUX_VERSION_IS_GEQ(3,18,17)
#include_next <linux/property.h>
#endif

#if LINUX_VERSION_IS_LESS(4,3,0)

#define device_get_mac_address LINUX_I915_BACKPORT(device_get_mac_address)
void *device_get_mac_address(struct device *dev, char *addr, int alen);

#endif /* < 4.3 */

#endif /* __BACKPORT_LINUX_PROPERTY_H_ */
