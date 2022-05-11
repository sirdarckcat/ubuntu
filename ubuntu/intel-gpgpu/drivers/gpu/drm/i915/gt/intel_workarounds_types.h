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
 * Copyright © 2014-2018 Intel Corporation
 */

#ifndef __INTEL_WORKAROUNDS_TYPES_H__
#define __INTEL_WORKAROUNDS_TYPES_H__

#include <linux/types.h>

#include "i915_reg_defs.h"

struct i915_whitelist_reg {
	i915_reg_t reg;
	u32 flags;
};

struct i915_wa {
	i915_reg_t	reg;
	u32		clr;
	u32		set;
	u32		read;
	bool		masked_reg;
};

struct i915_wa_list {
	struct intel_gt *gt;
	const char	*name;
	const char	*engine_name;
	struct i915_wa	*list;
	unsigned int	count;
	unsigned int	wa_count;
};

#endif /* __INTEL_WORKAROUNDS_TYPES_H__ */
