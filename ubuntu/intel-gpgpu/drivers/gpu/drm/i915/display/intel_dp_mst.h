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
 * Copyright © 2019 Intel Corporation
 */

#ifndef __INTEL_DP_MST_H__
#define __INTEL_DP_MST_H__

#include <linux/types.h>

struct intel_crtc_state;
struct intel_digital_port;
struct intel_dp;

int intel_dp_mst_encoder_init(struct intel_digital_port *dig_port, int conn_id);
void intel_dp_mst_encoder_cleanup(struct intel_digital_port *dig_port);
int intel_dp_mst_encoder_active_links(struct intel_digital_port *dig_port);
bool intel_dp_mst_is_master_trans(const struct intel_crtc_state *crtc_state);
bool intel_dp_mst_is_slave_trans(const struct intel_crtc_state *crtc_state);
bool intel_dp_mst_source_support(struct intel_dp *intel_dp);

#endif /* __INTEL_DP_MST_H__ */
