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

#ifndef __VLV_DSI_H__
#define __VLV_DSI_H__

#include <linux/types.h>

enum port;
struct drm_i915_private;
struct intel_dsi;

void vlv_dsi_wait_for_fifo_empty(struct intel_dsi *intel_dsi, enum port port);
enum mipi_dsi_pixel_format pixel_format_from_register_bits(u32 fmt);
void vlv_dsi_init(struct drm_i915_private *dev_priv);

#endif /* __VLV_DSI_H__ */
