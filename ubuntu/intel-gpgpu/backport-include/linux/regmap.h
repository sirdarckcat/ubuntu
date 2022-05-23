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
#ifndef __BACKPORT_LINUX_REGMAP_H
#define __BACKPORT_LINUX_REGMAP_H
#include_next <linux/regmap.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,5,0) && \
    LINUX_VERSION_IS_GEQ(3,2,0)
#define dev_get_regmap LINUX_I915_BACKPORT(dev_get_regmap)
static inline
struct regmap *dev_get_regmap(struct device *dev, const char *name)
{
	return NULL;
}
#endif

#if LINUX_VERSION_IS_LESS(3,4,0) && \
    LINUX_VERSION_IS_GEQ(3,2,0)
#if defined(CONFIG_REGMAP)
#define devm_regmap_init LINUX_I915_BACKPORT(devm_regmap_init)
struct regmap *devm_regmap_init(struct device *dev,
				const struct regmap_bus *bus,
				const struct regmap_config *config);
#if defined(CONFIG_REGMAP_I2C)
#define devm_regmap_init_i2c LINUX_I915_BACKPORT(devm_regmap_init_i2c)
struct regmap *devm_regmap_init_i2c(struct i2c_client *i2c,
				    const struct regmap_config *config);
#endif /* defined(CONFIG_REGMAP_I2C) */

/*
 * We can't backport these unless we try to backport
 * the full regmap into core so warn if used.
 * No drivers are using this yet anyway.
 */
#define regmap_raw_write_async LINUX_I915_BACKPORT(regmap_raw_write_async)
static inline int regmap_raw_write_async(struct regmap *map, unsigned int reg,
					 const void *val, size_t val_len)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

#define regmap_async_complete LINUX_I915_BACKPORT(regmap_async_complete)
static inline void regmap_async_complete(struct regmap *map)
{
	WARN_ONCE(1, "regmap API is disabled");
}

#endif /* defined(CONFIG_REGMAP) */
#endif /* 3.2 <= version < 3.4 */

#endif /* __BACKPORT_LINUX_REGMAP_H */
