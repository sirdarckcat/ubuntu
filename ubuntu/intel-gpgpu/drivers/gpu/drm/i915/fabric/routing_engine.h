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
 * Copyright(c) 2019 - 2021 Intel Corporation.
 *
 */

#ifndef ROUTING_ENGINE_H_INCLUDED
#define ROUTING_ENGINE_H_INCLUDED

#include "routing_topology.h"

void routing_init(void);
void routing_stop(void);
void routing_destroy(void);
void routing_sweep(u64 serviced_requests);
void routing_sd_init(struct fsubdev *sd);
void routing_sd_destroy(struct fsubdev *sd);
int routing_sd_once(struct fsubdev *sd);
void routing_dev_unroute(struct fdev *dev);

void routing_port_routed_query(struct fsubdev *sd, unsigned long *port_mask,
			       unsigned long *usage_mask);
void routing_generation_read(u32 *counter_start, u32 *counter_end);

#endif /* ROUTING_ENGINE_H_INCLUDED */
