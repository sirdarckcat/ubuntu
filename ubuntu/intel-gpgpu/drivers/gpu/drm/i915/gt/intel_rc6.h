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

#ifndef INTEL_RC6_H
#define INTEL_RC6_H

#include "i915_reg_defs.h"

struct intel_engine_cs;
struct intel_rc6;

void intel_rc6_rpm_get(struct intel_rc6 *rc6);
void intel_rc6_rpm_put(struct intel_rc6 *rc6);

void intel_rc6_init(struct intel_rc6 *rc6);
void intel_rc6_fini(struct intel_rc6 *rc6);

void intel_rc6_unpark(struct intel_rc6 *rc6);
void intel_rc6_park(struct intel_rc6 *rc6);

void intel_rc6_sanitize(struct intel_rc6 *rc6);
void intel_rc6_enable(struct intel_rc6 *rc6);
void intel_rc6_disable(struct intel_rc6 *rc6);

u64 intel_rc6_residency_ns(struct intel_rc6 *rc6, i915_reg_t reg);
u64 intel_rc6_residency_us(struct intel_rc6 *rc6, i915_reg_t reg);

#endif /* INTEL_RC6_H */
