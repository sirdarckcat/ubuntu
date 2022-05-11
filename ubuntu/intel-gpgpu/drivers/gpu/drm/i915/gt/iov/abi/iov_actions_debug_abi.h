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
 * Copyright © 2022 Intel Corporation
 */

#ifndef _ABI_IOV_ACTIONS_DEBUG_ABI_H_
#define _ABI_IOV_ACTIONS_DEBUG_ABI_H_

#include "iov_actions_abi.h"

/**
 * DOC: IOV debug actions
 *
 * These range of IOV action codes is reserved for debug and may only be
 * used on selected debug configs.
 *
 *  _`IOV_ACTION_DEBUG_ONLY_START` = 0xDEB0
 *  _`IOV_ACTION_DEBUG_ONLY_END` = 0xDEFF
 */

#define IOV_ACTION_DEBUG_ONLY_START	0xDEB0
#define IOV_ACTION_DEBUG_ONLY_END	0xDEFF

#endif /* _ABI_IOV_ACTIONS_DEBUG_ABI_H_ */
