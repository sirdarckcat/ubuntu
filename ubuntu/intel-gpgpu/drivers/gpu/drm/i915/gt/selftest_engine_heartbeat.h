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

#ifndef SELFTEST_ENGINE_HEARTBEAT_H
#define SELFTEST_ENGINE_HEARTBEAT_H

struct intel_engine_cs;

void st_engine_heartbeat_disable(struct intel_engine_cs *engine);
void st_engine_heartbeat_disable_no_pm(struct intel_engine_cs *engine);
void st_engine_heartbeat_enable(struct intel_engine_cs *engine);
void st_engine_heartbeat_enable_no_pm(struct intel_engine_cs *engine);

#endif /* SELFTEST_ENGINE_HEARTBEAT_H */
