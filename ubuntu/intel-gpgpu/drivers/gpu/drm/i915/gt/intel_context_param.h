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

#ifndef INTEL_CONTEXT_PARAM_H
#define INTEL_CONTEXT_PARAM_H

#include <linux/types.h>

#include "intel_context.h"

int intel_context_set_ring_size(struct intel_context *ce, long sz);
long intel_context_get_ring_size(struct intel_context *ce);

static inline int
intel_context_set_watchdog_us(struct intel_context *ce, u64 timeout_us)
{
	ce->watchdog.timeout_us = timeout_us;
	return 0;
}

#endif /* INTEL_CONTEXT_PARAM_H */
