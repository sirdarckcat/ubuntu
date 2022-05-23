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
#ifndef __BACKPORT_LINUX_CLK_H
#define __BACKPORT_LINUX_CLK_H
#include_next <linux/clk.h>
#include <linux/version.h>

/*
 * commit 93abe8e4 - we only backport the non CONFIG_COMMON_CLK
 * case as the CONFIG_COMMON_CLK case requires arch support. By
 * using the backport_ namespace for older kernels we force usage
 * of these helpers and that's required given that 3.5 added some
 * of these helpers expecting a few exported symbols for the non
 * CONFIG_COMMON_CLK case. The 3.5 kernel is not supported as
 * per kernel.org so we don't send a fix upstream for that.
 */
#if LINUX_VERSION_IS_LESS(3,6,0)

#ifndef CONFIG_COMMON_CLK

/*
 * Whoopsie!
 *
 * clk_enable() and clk_disable() have been left without
 * a nop export symbols when !CONFIG_COMMON_CLK since its
 * introduction on v2.6.16, but fixed until 3.6.
 */
#if 0
#define clk_enable LINUX_I915_BACKPORT(clk_enable)
static inline int clk_enable(struct clk *clk)
{
	return 0;
}

#define clk_disable LINUX_I915_BACKPORT(clk_disable)
static inline void clk_disable(struct clk *clk) {}
#endif


#define clk_get LINUX_I915_BACKPORT(clk_get)
static inline struct clk *clk_get(struct device *dev, const char *id)
{
	return NULL;
}

#define devm_clk_get LINUX_I915_BACKPORT(devm_clk_get)
static inline struct clk *devm_clk_get(struct device *dev, const char *id)
{
	return NULL;
}

#define clk_put LINUX_I915_BACKPORT(clk_put)
static inline void clk_put(struct clk *clk) {}

#define devm_clk_put LINUX_I915_BACKPORT(devm_clk_put)
static inline void devm_clk_put(struct device *dev, struct clk *clk) {}

#define clk_get_rate LINUX_I915_BACKPORT(clk_get_rate)
static inline unsigned long clk_get_rate(struct clk *clk)
{
	return 0;
}

#define clk_set_rate LINUX_I915_BACKPORT(clk_set_rate)
static inline int clk_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

#define clk_round_rate LINUX_I915_BACKPORT(clk_round_rate)
static inline long clk_round_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

#define clk_set_parent LINUX_I915_BACKPORT(clk_set_parent)
static inline int clk_set_parent(struct clk *clk, struct clk *parent)
{
	return 0;
}

#define clk_get_parent LINUX_I915_BACKPORT(clk_get_parent)
static inline struct clk *clk_get_parent(struct clk *clk)
{
	return NULL;
}
#endif /* CONFIG_COMMON_CLK */

#endif /* #if LINUX_VERSION_IS_LESS(3,0,0) */

#if LINUX_VERSION_IS_LESS(3,3,0) && \
    LINUX_VERSION_IS_GEQ(3,2,0)
#define clk_prepare_enable LINUX_I915_BACKPORT(clk_prepare_enable)
/* clk_prepare_enable helps cases using clk_enable in non-atomic context. */
static inline int clk_prepare_enable(struct clk *clk)
{
	int ret;

	ret = clk_prepare(clk);
	if (ret)
		return ret;
	ret = clk_enable(clk);
	if (ret)
		clk_unprepare(clk);

	return ret;
}

#define clk_disable_unprepare LINUX_I915_BACKPORT(clk_disable_unprepare)
/* clk_disable_unprepare helps cases using clk_disable in non-atomic context. */
static inline void clk_disable_unprepare(struct clk *clk)
{
	clk_disable(clk);
	clk_unprepare(clk);
}
#endif /* < 3,3,0 && >= 3,2,0 */

#endif /* __LINUX_CLK_H */
