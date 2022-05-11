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
// SPDX-License-Identifier: MIT
/*
 * Copyright © 2020 Intel Corporation
 */

#ifndef __SYSFS_GT_H__
#define __SYSFS_GT_H__

#include <linux/ctype.h>
#include <linux/kobject.h>

#include "i915_gem.h" /* GEM_BUG_ON() */

struct intel_gt;

struct kobj_gt {
	struct kobject base;
	struct intel_gt *gt;
};

static inline bool is_object_gt(struct kobject *kobj)
{
	bool b = !strncmp(kobj->name, "gt", 2);

	GEM_BUG_ON(b && !isdigit(kobj->name[2]));

	return b;
}

struct kobject *
intel_gt_create_kobj(struct intel_gt *gt,
		     struct kobject *dir,
		     const char *name);

static inline struct intel_gt *kobj_to_gt(struct kobject *kobj)
{
	return container_of(kobj, struct kobj_gt, base)->gt;
}

void intel_gt_sysfs_register(struct intel_gt *gt);
void intel_gt_sysfs_unregister(struct intel_gt *gt);
struct intel_gt *intel_gt_sysfs_get_drvdata(struct device *dev,
					    const char *name);

#endif /* SYSFS_GT_H */
