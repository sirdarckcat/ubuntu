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
 * Copyright(c) 2020 - 2021 Intel Corporation.
 *
 */

#ifndef ROUTING_PAUSE_H_INCLUDED
#define ROUTING_PAUSE_H_INCLUDED

struct routing_pause_ctx;

struct routing_pause_ctx *routing_pause_init(void);
void routing_pause_start(struct routing_pause_ctx *ctx);
void routing_pause_end(struct routing_pause_ctx *ctx);

#endif /* ROUTING_PAUSE_H_INCLUDED */
