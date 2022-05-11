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

#ifndef ROUTING_EVENT_H_INCLUDED
#define ROUTING_EVENT_H_INCLUDED

#include <linux/types.h>

void rem_init(void);
void rem_shutting_down(void);
void rem_stop(void);
bool rem_request(void);
int rem_route_start(u64 *serviced_requests);
void rem_route_finish(void);

#endif /* ROUTING_EVENT_H_INCLUDED */
