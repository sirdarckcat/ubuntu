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
#ifndef __BACKPORT_LINUX_GPIO_H
#define __BACKPORT_LINUX_GPIO_H
#include_next <linux/gpio.h>

#if LINUX_VERSION_IS_LESS(3,5,0)
#define devm_gpio_request_one LINUX_I915_BACKPORT(devm_gpio_request_one)
#define devm_gpio_request LINUX_I915_BACKPORT(devm_gpio_request)
#ifdef CONFIG_GPIOLIB
int devm_gpio_request(struct device *dev, unsigned gpio, const char *label);
int devm_gpio_request_one(struct device *dev, unsigned gpio,
			  unsigned long flags, const char *label);
void devm_gpio_free(struct device *dev, unsigned int gpio);
#else
static inline int devm_gpio_request(struct device *dev, unsigned gpio,
				    const char *label)
{
	WARN_ON(1);
	return -EINVAL;
}

static inline int devm_gpio_request_one(struct device *dev, unsigned gpio,
					unsigned long flags, const char *label)
{
	WARN_ON(1);
	return -EINVAL;
}

static inline void devm_gpio_free(struct device *dev, unsigned int gpio)
{
	WARN_ON(1);
}
#endif /* CONFIG_GPIOLIB */
#endif

#endif /* __BACKPORT_LINUX_GPIO_H */
