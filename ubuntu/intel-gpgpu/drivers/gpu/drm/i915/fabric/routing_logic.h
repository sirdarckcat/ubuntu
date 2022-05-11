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

#ifndef ROUTING_LOGIC_H_INCLUDED
#define ROUTING_LOGIC_H_INCLUDED

#include "routing_topology.h"

u16 routing_cost_lookup(struct fsubdev *sd_src, struct fsubdev *sd_dst);
void routing_plane_destroy(struct routing_plane *plane);
int routing_logic_run(struct routing_topology *topo);

#endif /* ROUTING_LOGIC_H_INCLUDED */
