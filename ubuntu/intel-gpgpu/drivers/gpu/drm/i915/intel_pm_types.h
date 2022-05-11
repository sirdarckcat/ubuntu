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

#ifndef __INTEL_PM_TYPES_H__
#define __INTEL_PM_TYPES_H__

#include <linux/types.h>

#include "display/intel_display.h"

enum intel_ddb_partitioning {
	INTEL_DDB_PART_1_2,
	INTEL_DDB_PART_5_6, /* IVB+ */
};

struct ilk_wm_values {
	u32 wm_pipe[3];
	u32 wm_lp[3];
	u32 wm_lp_spr[3];
	bool enable_fbc_wm;
	enum intel_ddb_partitioning partitioning;
};

struct g4x_pipe_wm {
	u16 plane[I915_MAX_PLANES];
	u16 fbc;
};

struct g4x_sr_wm {
	u16 plane;
	u16 cursor;
	u16 fbc;
};

struct vlv_wm_ddl_values {
	u8 plane[I915_MAX_PLANES];
};

struct vlv_wm_values {
	struct g4x_pipe_wm pipe[3];
	struct g4x_sr_wm sr;
	struct vlv_wm_ddl_values ddl[3];
	u8 level;
	bool cxsr;
};

struct g4x_wm_values {
	struct g4x_pipe_wm pipe[2];
	struct g4x_sr_wm sr;
	struct g4x_sr_wm hpll;
	bool cxsr;
	bool hpll_en;
	bool fbc_en;
};

struct skl_ddb_entry {
	u16 start, end;	/* in number of blocks, 'end' is exclusive */
};

static inline u16 skl_ddb_entry_size(const struct skl_ddb_entry *entry)
{
	return entry->end - entry->start;
}

static inline bool skl_ddb_entry_equal(const struct skl_ddb_entry *e1,
				       const struct skl_ddb_entry *e2)
{
	if (e1->start == e2->start && e1->end == e2->end)
		return true;

	return false;
}

#endif /* __INTEL_PM_TYPES_H__ */
