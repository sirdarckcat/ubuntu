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

#ifndef _FW_H_
#define _FW_H_

#include "iaf_drv.h"

#define FW_VERSION_INIT_BIT	BIT(1)
#define FW_VERSION_ENV_BIT	BIT(0)

void fw_init_module(void);
void fw_init_dev(struct fdev *dev);
int load_and_init_fw(struct fdev *dev);
void flush_any_outstanding_fw_initializations(struct fdev *dev);
bool is_opcode_valid(struct fsubdev *sd, u8 op_code);

#endif
