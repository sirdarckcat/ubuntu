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

#ifndef INTEL_ENGINE_HEARTBEAT_H
#define INTEL_ENGINE_HEARTBEAT_H

struct intel_engine_cs;
struct intel_gt;

void intel_engine_init_heartbeat(struct intel_engine_cs *engine);

int intel_engine_set_heartbeat(struct intel_engine_cs *engine,
			       unsigned long delay);

void intel_engine_park_heartbeat(struct intel_engine_cs *engine);
void intel_engine_unpark_heartbeat(struct intel_engine_cs *engine);

void intel_gt_park_heartbeats(struct intel_gt *gt);
void intel_gt_unpark_heartbeats(struct intel_gt *gt);

int intel_engine_pulse(struct intel_engine_cs *engine);
int intel_engine_flush_barriers(struct intel_engine_cs *engine);

void intel_engine_schedule_heartbeat(struct intel_engine_cs *engine);

#endif /* INTEL_ENGINE_HEARTBEAT_H */
