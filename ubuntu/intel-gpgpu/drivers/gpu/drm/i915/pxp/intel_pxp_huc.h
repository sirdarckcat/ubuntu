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
 * Copyright(c) 2021, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_HUC_H__
#define __INTEL_PXP_HUC_H__

#include <linux/types.h>

struct intel_pxp;

int intel_pxp_huc_load_and_auth(struct intel_pxp *pxp);

#endif /* __INTEL_PXP_HUC_H__ */
