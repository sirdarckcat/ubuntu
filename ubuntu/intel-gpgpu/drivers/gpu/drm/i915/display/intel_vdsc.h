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

#ifndef __INTEL_VDSC_H__
#define __INTEL_VDSC_H__

#include <linux/types.h>

enum transcoder;
struct intel_crtc;
struct intel_crtc_state;
struct intel_encoder;

bool intel_dsc_source_support(const struct intel_crtc_state *crtc_state);
void intel_uncompressed_joiner_enable(const struct intel_crtc_state *crtc_state);
void intel_dsc_enable(const struct intel_crtc_state *crtc_state);
void intel_dsc_disable(const struct intel_crtc_state *crtc_state);
int intel_dsc_compute_params(struct intel_crtc_state *pipe_config);
void intel_dsc_get_config(struct intel_crtc_state *crtc_state);
enum intel_display_power_domain
intel_dsc_power_domain(struct intel_crtc *crtc, enum transcoder cpu_transcoder);
struct intel_crtc *intel_dsc_get_bigjoiner_secondary(const struct intel_crtc *primary_crtc);
void intel_dsc_dsi_pps_write(struct intel_encoder *encoder,
			     const struct intel_crtc_state *crtc_state);
void intel_dsc_dp_pps_write(struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state);

#endif /* __INTEL_VDSC_H__ */
