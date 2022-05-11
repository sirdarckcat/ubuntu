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
 * Copyright © 2020 Intel Corporation
 */

#ifndef _INTEL_DDI_BUF_TRANS_H_
#define _INTEL_DDI_BUF_TRANS_H_

#include <linux/types.h>

struct drm_i915_private;
struct intel_encoder;
struct intel_crtc_state;

struct hsw_ddi_buf_trans {
	u32 trans1;	/* balance leg enable, de-emph level */
	u32 trans2;	/* vref sel, vswing */
	u8 i_boost;	/* SKL: I_boost; valid: 0x0, 0x1, 0x3, 0x7 */
};

struct bxt_ddi_buf_trans {
	u8 margin;	/* swing value */
	u8 scale;	/* scale value */
	u8 enable;	/* scale enable */
	u8 deemphasis;
};

struct icl_ddi_buf_trans {
	u8 dw2_swing_sel;
	u8 dw7_n_scalar;
	u8 dw4_cursor_coeff;
	u8 dw4_post_cursor_2;
	u8 dw4_post_cursor_1;
};

struct icl_mg_phy_ddi_buf_trans {
	u8 cri_txdeemph_override_11_6;
	u8 cri_txdeemph_override_5_0;
	u8 cri_txdeemph_override_17_12;
};

struct tgl_dkl_phy_ddi_buf_trans {
	u8 vswing;
	u8 preshoot;
	u8 de_emphasis;
};

struct dg2_snps_phy_buf_trans {
	u8 vswing;
	u8 pre_cursor;
	u8 post_cursor;
};

union intel_ddi_buf_trans_entry {
	struct hsw_ddi_buf_trans hsw;
	struct bxt_ddi_buf_trans bxt;
	struct icl_ddi_buf_trans icl;
	struct icl_mg_phy_ddi_buf_trans mg;
	struct tgl_dkl_phy_ddi_buf_trans dkl;
	struct dg2_snps_phy_buf_trans snps;
};

struct intel_ddi_buf_trans {
	const union intel_ddi_buf_trans_entry *entries;
	u8 num_entries;
	u8 hdmi_default_entry;
};

bool is_hobl_buf_trans(const struct intel_ddi_buf_trans *table);

void intel_ddi_buf_trans_init(struct intel_encoder *encoder);

#endif
