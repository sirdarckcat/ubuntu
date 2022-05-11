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

#ifndef _INTEL_GUC_RC_H_
#define _INTEL_GUC_RC_H_

#include "intel_guc_submission.h"

void intel_guc_rc_init_early(struct intel_guc *guc);

static inline bool intel_guc_rc_is_supported(const struct intel_guc *guc)
{
	return guc->rc_supported;
}

static inline bool intel_guc_rc_is_wanted(const struct intel_guc *guc)
{
	return guc->submission_selected && intel_guc_rc_is_supported(guc);
}

static inline bool intel_guc_rc_is_used(const struct intel_guc *guc)
{
	return intel_guc_submission_is_used(guc) && intel_guc_rc_is_wanted(guc);
}

int intel_guc_rc_enable(struct intel_guc *guc);
int intel_guc_rc_disable(struct intel_guc *guc);

#endif
