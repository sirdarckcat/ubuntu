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
#ifndef __BACKPORT_OF_IRQ_H
#define __BACKPORT_OF_IRQ_H
#include_next <linux/of_irq.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,5,0) && !defined(CONFIG_OF)
#define irq_of_parse_and_map LINUX_I915_BACKPORT(irq_of_parse_and_map)
static inline unsigned int irq_of_parse_and_map(struct device_node *dev,
						int index)
{
	return 0;
}
#endif /* LINUX_VERSION_IS_LESS(4,5,0) */

#endif /* __BACKPORT_OF_IRQ_H */
