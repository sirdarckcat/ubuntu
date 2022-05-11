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

#ifndef ROUTING_P2P_H_INCLUDED
#define ROUTING_P2P_H_INCLUDED

#include "routing_topology.h"

struct routing_p2p_entry;

void routing_p2p_init(struct dentry *root);

void routing_p2p_cache(void);
void routing_p2p_lookup(struct fdev *src, struct fdev *dst,
			struct query_info *qi);

void routing_p2p_clear(struct fdev *dev);

#endif /* ROUTING_P2P_H_INCLUDED */
