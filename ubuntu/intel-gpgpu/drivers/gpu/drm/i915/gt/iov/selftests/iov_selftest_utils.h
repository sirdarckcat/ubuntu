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
 * Copyright © 2021 Intel Corporation
 */

#ifndef _IOV_SELFTEST_UTILS_H_
#define _IOV_SELFTEST_UTILS_H_

#include <linux/types.h>

struct intel_iov;

bool intel_iov_check_ggtt_vfid(struct intel_iov *iov, void __iomem *pte_addr, u16 vfid);

#endif /* _IOV_SELFTEST_UTILS_H_ */
