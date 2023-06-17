// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 */

#include "regs/xe_gt_regs.h"
#include "xe_device.h"
#include "xe_force_wake.h"
#include "xe_gt.h"
#include "xe_gt_topology.h"
#include "xe_gt_debug.h"
#include "xe_gt_mcr.h"
#include "xe_pm.h"
#include "xe_macros.h"

static int xe_gt_foreach_dss_group_instance(struct xe_gt *gt,
					    int (*fn)(struct xe_gt *gt,
						      void *data,
						      u16 group,
						      u16 instance),
					    void *data)
{
	const enum xe_force_wake_domains fw_domains = XE_FW_GT;
	unsigned int dss, fw_ref;
	u16 group, instance;
	int ret = 0;

	fw_ref = xe_force_wake_get(gt_to_fw(gt), fw_domains);
	if (!fw_ref)
		return -ETIMEDOUT;

	for_each_dss_steering(dss, gt, group, instance) {
		ret = fn(gt, data, group, instance);
		if (ret)
			break;
	}

	xe_force_wake_put(gt_to_fw(gt), fw_ref);

	return ret;
}

static int read_first_attention_mcr(struct xe_gt *gt, void *data,
				    u16 group, u16 instance)
{
	unsigned int row;

	for (row = 0; row < 2; row++) {
		u32 val;

		val = xe_gt_mcr_unicast_read(gt, TD_ATT(row), group, instance);

		if (val)
			return 1;
	}

	return 0;
}

#define MAX_EUS_PER_ROW 4u
#define MAX_THREADS 8u

/**
 * xe_gt_eu_attention_bitmap_size - query size of the attention bitmask
 *
 * @gt: pointer to struct xe_gt
 *
 * Return: size in bytes.
 */
int xe_gt_eu_attention_bitmap_size(struct xe_gt *gt)
{
	xe_dss_mask_t dss_mask;

	bitmap_or(dss_mask, gt->fuse_topo.c_dss_mask,
		  gt->fuse_topo.g_dss_mask, XE_MAX_DSS_FUSE_BITS);

	return  bitmap_weight(dss_mask, XE_MAX_DSS_FUSE_BITS) *
		TD_EU_ATTENTION_MAX_ROWS * MAX_THREADS *
		MAX_EUS_PER_ROW / 8;
}

struct attn_read_iter {
	struct xe_gt *gt;
	unsigned int i;
	unsigned int size;
	u8 *bits;
};

static int read_eu_attentions_mcr(struct xe_gt *gt, void *data,
				  u16 group, u16 instance)
{
	struct attn_read_iter * const iter = data;
	unsigned int row;

	for (row = 0; row < TD_EU_ATTENTION_MAX_ROWS; row++) {
		u32 val;

		if (iter->i >= iter->size)
			return 0;

		XE_WARN_ON(iter->i + sizeof(val) > xe_gt_eu_attention_bitmap_size(gt));

		val = xe_gt_mcr_unicast_read(gt, TD_ATT(row), group, instance);

		memcpy(&iter->bits[iter->i], &val, sizeof(val));
		iter->i += sizeof(val);
	}

	return 0;
}

/**
 * xe_gt_eu_attention_bitmap - query host attention
 *
 * @gt: pointer to struct xe_gt
 *
 * Return: 0 on success, negative otherwise.
 */
int xe_gt_eu_attention_bitmap(struct xe_gt *gt, u8 *bits,
			      unsigned int bitmap_size)
{
	struct attn_read_iter iter = {
		.gt = gt,
		.i = 0,
		.size = bitmap_size,
		.bits = bits
	};

	return xe_gt_foreach_dss_group_instance(gt, read_eu_attentions_mcr, &iter);
}

/**
 * xe_gt_eu_threads_needing_attention - Query host attention
 *
 * @gt: pointer to struct xe_gt
 *
 * Return: 1 if threads waiting host attention, 0 otherwise.
 */
int xe_gt_eu_threads_needing_attention(struct xe_gt *gt)
{
	int err;

	err = xe_gt_foreach_dss_group_instance(gt, read_first_attention_mcr, NULL);

	XE_WARN_ON(err < 0);

	return err < 0 ? 0 : err;
}
