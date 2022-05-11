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

#include <drm/drm_print.h>

#include "i915_drv.h" /* for_each_engine! */
#include "intel_engine.h"
#include "intel_gt_debugfs.h"
#include "intel_gt_engines_debugfs.h"

static int engines_show(struct seq_file *m, void *data)
{
	struct intel_gt *gt = m->private;
	struct intel_engine_cs *engine;
	enum intel_engine_id id;
	struct drm_printer p;

	p = drm_seq_file_printer(m);
	for_each_engine(engine, gt, id)
		intel_engine_dump(engine, &p, "%s\n", engine->name);

	return 0;
}
DEFINE_INTEL_GT_DEBUGFS_ATTRIBUTE(engines);

void intel_gt_engines_debugfs_register(struct intel_gt *gt, struct dentry *root)
{
	static const struct intel_gt_debugfs_file files[] = {
		{ "engines", &engines_fops },
	};

	intel_gt_debugfs_register_files(root, files, ARRAY_SIZE(files), gt);
}
