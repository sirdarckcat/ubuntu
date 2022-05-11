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
/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2021 Intel Corporation
 */

#ifndef _INTEL_GUC_SLPC_TYPES_H_
#define _INTEL_GUC_SLPC_TYPES_H_

#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/types.h>

#define SLPC_RESET_TIMEOUT_MS 5

struct intel_guc_slpc {
	struct i915_vma *vma;
	struct slpc_shared_data *vaddr;
	bool supported;
	bool selected;

	/* Indicates this is a server part */
	bool min_is_rpmax;

	/* platform frequency limits */
	u32 min_freq;
	u32 rp0_freq;
	u32 rp1_freq;
	u32 boost_freq;

	/* frequency softlimits */
	u32 min_freq_softlimit;
	u32 max_freq_softlimit;

	/* cached media ratio mode */
	u32 media_ratio_mode;

	/* Protects set/reset of boost freq
	 * and value of num_waiters
	 */
	struct mutex lock;

	struct work_struct boost_work;
	atomic_t num_waiters;
	u32 num_boosts;
};

#endif
