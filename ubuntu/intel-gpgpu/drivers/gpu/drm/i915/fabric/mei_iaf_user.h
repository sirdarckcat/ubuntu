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
 */

#ifndef _MEI_IAF_USER_H_
#define _MEI_IAF_USER_H_

#include <linux/device.h>

#include "iaf_drv.h"

int iaf_mei_start(struct fdev *dev);
void iaf_mei_stop(struct fdev *dev);
void iaf_mei_indicate_device_ok(struct fdev *dev);
int iaf_commit_svn(struct fdev *dev);
u16 get_min_svn(struct fdev *dev);

#endif
