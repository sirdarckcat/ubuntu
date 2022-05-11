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
#ifndef __BACKPORT_PM_RUNTIME_H
#define __BACKPORT_PM_RUNTIME_H
#include_next <linux/pm_runtime.h>

#if LINUX_VERSION_IS_LESS(3,9,0)
#define pm_runtime_active LINUX_I915_BACKPORT(pm_runtime_active)
#ifdef CONFIG_PM
static inline bool pm_runtime_active(struct device *dev)
{
	return dev->power.runtime_status == RPM_ACTIVE
		|| dev->power.disable_depth;
}
#else
static inline bool pm_runtime_active(struct device *dev) { return true; }
#endif /* CONFIG_PM */

#endif /* LINUX_VERSION_IS_LESS(3,9,0) */

#if LINUX_VERSION_IS_LESS(3,15,0)
static inline int pm_runtime_force_suspend(struct device *dev)
{
#ifdef CONFIG_PM
	/* cannot backport properly, I think */
	WARN_ON_ONCE(1);
	return -EINVAL;
#endif
	return 0;
}
static inline int pm_runtime_force_resume(struct device *dev)
{
#ifdef CONFIG_PM
	/* cannot backport properly, I think */
	WARN_ON_ONCE(1);
	return -EINVAL;
#endif
	return 0;
}
#endif

#endif /* __BACKPORT_PM_RUNTIME_H */
