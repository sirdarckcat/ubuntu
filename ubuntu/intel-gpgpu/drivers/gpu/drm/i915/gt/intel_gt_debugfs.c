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
 * Copyright © 2019 Intel Corporation
 */

#include <linux/debugfs.h>

#include "i915_drv.h"
#include "intel_gt.h"
#include "intel_gt_debugfs.h"
#include "intel_gt_engines_debugfs.h"
#include "intel_gt_pm_debugfs.h"
#include "intel_sseu_debugfs.h"
#include "iov/intel_iov_debugfs.h"
#include "uc/intel_uc_debugfs.h"

int intel_gt_debugfs_reset_show(struct intel_gt *gt, u64 *val)
{
	int ret = intel_gt_terminally_wedged(gt);

	switch (ret) {
	case -EIO:
		*val = 1;
		return 0;
	case 0:
		*val = 0;
		return 0;
	default:
		return ret;
	}
}

void intel_gt_debugfs_reset_store(struct intel_gt *gt, u64 val)
{
	/* Flush any previous reset before applying for a new one */
	wait_event(gt->reset.queue,
		   !test_bit(I915_RESET_BACKOFF, &gt->reset.flags));

	intel_gt_handle_error(gt, val, I915_ERROR_CAPTURE,
			      "Manually reset engine mask to %llx", val);
}

/*
 * keep the interface clean where the first parameter
 * is a 'struct intel_gt *' instead of 'void *'
 */
static int __intel_gt_debugfs_reset_show(void *data, u64 *val)
{
	return intel_gt_debugfs_reset_show(data, val);
}

static int __intel_gt_debugfs_reset_store(void *data, u64 val)
{
	intel_gt_debugfs_reset_store(data, val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reset_fops, __intel_gt_debugfs_reset_show,
			__intel_gt_debugfs_reset_store, "%llu\n");

static int steering_show(struct seq_file *m, void *data)
{
	struct drm_printer p = drm_seq_file_printer(m);
	struct intel_gt *gt = m->private;

	intel_gt_report_steering(&p, gt, true);

	return 0;
}
DEFINE_INTEL_GT_DEBUGFS_ATTRIBUTE(steering);

static int fake_int_slow_get(void *data, u64 *val)
{
	struct intel_gt *gt = data;

	if (!gt->fake_int.enabled)
		return -ENODEV;

	*val = gt->fake_int.delay_slow;

	return 0;
}

static int fake_int_slow_set(void *data, u64 val)
{
	struct intel_gt *gt = data;

	if (!gt->fake_int.enabled)
		return -ENODEV;

	gt->fake_int.delay_slow = val;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fake_int_slow_fops, fake_int_slow_get, fake_int_slow_set, "%llu\n");

static int fake_int_fast_get(void *data, u64 *val)
{
	struct intel_gt *gt = data;

	if (!gt->fake_int.enabled)
		return -ENODEV;

	*val = gt->fake_int.delay_fast;

	return 0;
}

static int fake_int_fast_set(void *data, u64 val)
{
	struct intel_gt *gt = data;

	if (!gt->fake_int.enabled)
		return -ENODEV;

	gt->fake_int.delay_fast = val;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fake_int_fast_fops, fake_int_fast_get, fake_int_fast_set, "%llu\n");

static int debug_pages_show(struct seq_file *m, void *data)
{
	struct intel_gt *gt = m->private;

	if (gt->dbg) {
		u32* vaddr;
		int i;
		seq_printf(m, "debug pages allocated in %s: "
				"ggtt=0x%08x, phys=0x%016llx, size=0x%zx\n\n",
				gt->dbg->obj->mm.region->name,
				i915_ggtt_offset(gt->dbg),
				(u64)i915_gem_object_get_dma_address(gt->dbg->obj, 0),
				gt->dbg->obj->base.size);

		vaddr = i915_gem_object_pin_map_unlocked(gt->dbg->obj, I915_MAP_WC);
		if (!vaddr)
			return -ENOSPC;

		for (i = 0; i < (gt->dbg->obj->base.size / sizeof(u32)); i += 4)
			seq_printf(m, "[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				   i * 4, vaddr[i], vaddr[i + 1], vaddr[i + 2], vaddr[i + 3]);

		i915_gem_object_unpin_map(gt->dbg->obj);
	}

	return 0;
}
DEFINE_INTEL_GT_DEBUGFS_ATTRIBUTE(debug_pages);

static void gt_debugfs_register(struct intel_gt *gt, struct dentry *root)
{
	static const struct intel_gt_debugfs_file files[] = {
		{ "reset", &reset_fops, NULL },
		{ "steering", &steering_fops },
		{ "fake_int_slow_ns", &fake_int_slow_fops, NULL },
		{ "fake_int_fast_ns", &fake_int_fast_fops, NULL },
		{ "debug_pages", &debug_pages_fops, NULL },
	};

	intel_gt_debugfs_register_files(root, files, ARRAY_SIZE(files), gt);
}

void intel_gt_debugfs_register(struct intel_gt *gt)
{
	struct dentry *root;
	char gtname[4];

	if (!gt->i915->drm.primary->debugfs_root)
		return;

	snprintf(gtname, sizeof(gtname), "gt%u", gt->info.id);
	root = debugfs_create_dir(gtname, gt->i915->drm.primary->debugfs_root);
	if (IS_ERR(root))
		return;

	gt_debugfs_register(gt, root);

	intel_gt_engines_debugfs_register(gt, root);
	intel_gt_pm_debugfs_register(gt, root);
	intel_sseu_debugfs_register(gt, root);

	intel_uc_debugfs_register(&gt->uc, root);
	intel_iov_debugfs_register(&gt->iov, root);
}

void intel_gt_debugfs_register_files(struct dentry *root,
				     const struct intel_gt_debugfs_file *files,
				     unsigned long count, void *data)
{
	while (count--) {
		umode_t mode = files->fops->write ? 0644 : 0444;

		if (!files->eval || files->eval(data))
			debugfs_create_file(files->name,
					    mode, root, data,
					    files->fops);

		files++;
	}
}
