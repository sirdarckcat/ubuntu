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
#ifndef __BACKPORT_LINUX_IRQ_H
#define __BACKPORT_LINUX_IRQ_H
#include_next <linux/irq.h>

#ifdef CONFIG_HAVE_GENERIC_HARDIRQS
#if LINUX_VERSION_IS_LESS(3,11,0)
#define irq_get_trigger_type LINUX_I915_BACKPORT(irq_get_trigger_type)
static inline u32 irq_get_trigger_type(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	return d ? irqd_get_trigger_type(d) : 0;
}
#endif
#endif /* CONFIG_HAVE_GENERIC_HARDIRQS */

#endif /* __BACKPORT_LINUX_IRQ_H */
