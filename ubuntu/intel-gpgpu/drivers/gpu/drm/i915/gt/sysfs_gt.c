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

#include <drm/drm_device.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/printk.h>
#include <linux/sysfs.h>

#include "i915_drv.h"
#include "i915_sysfs.h"
#include "intel_gt.h"
#include "intel_gt_types.h"
#include "intel_rc6.h"

#include "intel_sysfs_mem_health.h"
#include "sysfs_gt.h"
#include "sysfs_gt_errors.h"
#include "sysfs_gt_pm.h"

struct intel_gt *intel_gt_sysfs_get_drvdata(struct device *dev,
					    const char *name)
{
	struct kobject *kobj = &dev->kobj;

	/*
	 * We are interested at knowing from where the interface
	 * has been called, whether it's called from gt/ or from
	 * the parent directory.
	 * From the interface position it depends also the value of
	 * the private data.
	 * If the interface is called from gt/ then private data is
	 * of the "struct intel_gt *" type, otherwise it's * a
	 * "struct drm_i915_private *" type.
	 */
	if (!is_object_gt(kobj)) {
		struct drm_i915_private *i915 = kdev_minor_to_i915(dev);

		pr_devel_ratelimited(DEPRECATED
			"%s (pid %d) is trying to access deprecated %s "
			"sysfs control, please use use gt/gt<n>/%s instead\n",
			current->comm, task_pid_nr(current), name, name);
		return to_gt(i915);
	}

	return kobj_to_gt(kobj);
}

static struct kobject *gt_get_parent_obj(struct intel_gt *gt)
{
	return &gt->i915->drm.primary->kdev->kobj;
}

static ssize_t id_show(struct device *dev,
		       struct device_attribute *attr,
		       char *buf)
{
	struct intel_gt *gt = intel_gt_sysfs_get_drvdata(dev, attr->attr.name);

	return sysfs_emit(buf, "%u\n", gt->info.id);
}

static DEVICE_ATTR_RO(id);

static void kobj_gt_release(struct kobject *kobj)
{
	kfree(kobj);
}

static struct kobj_type kobj_gt_type = {
	.release = kobj_gt_release,
	.sysfs_ops = &kobj_sysfs_ops
};

struct kobject *
intel_gt_create_kobj(struct intel_gt *gt, struct kobject *dir, const char *name)
{
	struct kobj_gt *kg;

	kg = kzalloc(sizeof(*kg), GFP_KERNEL);
	if (!kg)
		return NULL;

	kobject_init(&kg->base, &kobj_gt_type);
	kg->gt = gt;

	/* xfer ownership to sysfs tree */
	if (kobject_add(&kg->base, dir, "%s", name)) {
		kobject_put(&kg->base);
		return NULL;
	}

	return &kg->base; /* borrowed ref */
}

void intel_gt_sysfs_register(struct intel_gt *gt)
{
	struct kobject *dir;
	char name[80];

	/*
	 * We need to make things right with the
	 * ABI compatibility. The files were originally
	 * generated under the parent directory.
	 *
	 * We generate the files only for gt 0
	 * to avoid duplicates.
	 */
	if (!gt->info.id)
		intel_gt_sysfs_pm_init(gt, gt_get_parent_obj(gt));

	snprintf(name, sizeof(name), "gt%d", gt->info.id);

	dir = intel_gt_create_kobj(gt, gt->i915->sysfs_gt, name);
	if (!dir) {
		drm_err(&gt->i915->drm,
			"failed to initialize %s sysfs root\n", name);
		return;
	}

	gt->sysfs_defaults = kobject_create_and_add(".defaults", dir);
	if (!gt->sysfs_defaults) {
		drm_err(&gt->i915->drm, "failed to create gt sysfs .defaults\n");
		return;
	}

	if (sysfs_create_file(dir, &dev_attr_id.attr))
		drm_err(&gt->i915->drm,
			"failed to create sysfs %s info files\n", name);

	intel_gt_sysfs_pm_init(gt, dir);
	intel_gt_sysfs_register_errors(gt, dir);
	intel_gt_sysfs_register_mem(gt, dir);
}

void intel_gt_sysfs_unregister(struct intel_gt *gt)
{
}
