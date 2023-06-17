/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef __XE_GT_DEBUG_
#define __XE_GT_DEBUG_

#include "xe_gt_types.h"

#define XE_GT_ATTENTION_TIMEOUT_MS 100
#define XE_GT_EU_ATT_ROWS 2u

int xe_gt_eu_threads_needing_attention(struct xe_gt *gt);

int xe_gt_eu_attention_bitmap_size(struct xe_gt *gt);
int xe_gt_eu_attention_bitmap(struct xe_gt *gt, u8 *bits,
			      unsigned int bitmap_size);

#endif
