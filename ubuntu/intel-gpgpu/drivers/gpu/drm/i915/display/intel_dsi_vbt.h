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

#ifndef __INTEL_DSI_VBT_H__
#define __INTEL_DSI_VBT_H__

#include <linux/types.h>

enum mipi_seq;
struct intel_dsi;

bool intel_dsi_vbt_init(struct intel_dsi *intel_dsi, u16 panel_id);
void intel_dsi_vbt_gpio_init(struct intel_dsi *intel_dsi, bool panel_is_on);
void intel_dsi_vbt_gpio_cleanup(struct intel_dsi *intel_dsi);
void intel_dsi_vbt_exec_sequence(struct intel_dsi *intel_dsi,
				 enum mipi_seq seq_id);
void intel_dsi_msleep(struct intel_dsi *intel_dsi, int msec);
void intel_dsi_log_params(struct intel_dsi *intel_dsi);

#endif /* __INTEL_DSI_VBT_H__ */
