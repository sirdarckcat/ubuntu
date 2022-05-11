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
 * Copyright © 2014-2019 Intel Corporation
 */

#ifndef _INTEL_GUC_ADS_H_
#define _INTEL_GUC_ADS_H_

#include <linux/types.h>
#include <linux/iosys-map.h>

struct intel_guc;
struct drm_printer;
struct intel_engine_cs;

int intel_guc_ads_create(struct intel_guc *guc);
void intel_guc_ads_destroy(struct intel_guc *guc);
void intel_guc_ads_init_late(struct intel_guc *guc);
void intel_guc_ads_reset(struct intel_guc *guc);
void intel_guc_ads_print_policy_info(struct intel_guc *guc,
				     struct drm_printer *p);
struct iosys_map intel_guc_engine_usage_record_map(struct intel_engine_cs *engine);
u32 intel_guc_engine_usage_offset(struct intel_guc *guc);

#endif
