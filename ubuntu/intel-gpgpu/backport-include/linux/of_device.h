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
#ifndef __BP_OF_DEVICE_H
#define __BP_OF_DEVICE_H
#include_next <linux/of_device.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,18,0)
static inline int backport_of_dma_configure(struct device *dev,
					    struct device_node *np,
					    bool force_dma)
{
#if LINUX_VERSION_IS_GEQ(4,15,0)
	dev->bus->force_dma = force_dma;
	return of_dma_configure(dev, np);
#elif LINUX_VERSION_IS_GEQ(4,12,0)
	return of_dma_configure(dev, np);
#elif LINUX_VERSION_IS_GEQ(4,1,0)
	of_dma_configure(dev, np);
	return 0;
#else
	return 0;
#endif
}
#define of_dma_configure LINUX_I915_BACKPORT(of_dma_configure)
#endif /* < 4.18 */

#endif /* __BP_OF_DEVICE_H */
