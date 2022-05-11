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

#ifndef SELFTESTS_ROUTING_MOCK_H_INCLUDED
#define SELFTESTS_ROUTING_MOCK_H_INCLUDED

#if IS_ENABLED(CPTCFG_IAF_DEBUG_SELFTESTS)
#include "../routing_topology.h"

#define PORT_FABRIC_START      1
#define PORT_FABRIC_END        8
#define PORT_BRIDGE_START      9
#define PORT_BRIDGE_END        (PORT_COUNT - 1)

int routing_mock_create_topology(struct routing_topology *topo);
void routing_mock_destroy(struct routing_topology *topo);
#endif

#endif /* SELFTESTS_ROUTING_MOCK_H_INCLUDED */
