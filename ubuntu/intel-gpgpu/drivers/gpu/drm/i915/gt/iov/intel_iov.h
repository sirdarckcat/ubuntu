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

#ifndef __INTEL_IOV_H__
#define __INTEL_IOV_H__

struct intel_iov;

void intel_iov_init_early(struct intel_iov *iov);
void intel_iov_release(struct intel_iov *iov);

int intel_iov_init_mmio(struct intel_iov *iov);
int intel_iov_init_ggtt(struct intel_iov *iov);
void intel_iov_fini_ggtt(struct intel_iov *iov);
int intel_iov_init(struct intel_iov *iov);
void intel_iov_fini(struct intel_iov *iov);

int intel_iov_init_hw(struct intel_iov *iov);
void intel_iov_fini_hw(struct intel_iov *iov);
int intel_iov_init_late(struct intel_iov *iov);

void intel_iov_vf_get_wakeref_wa(struct intel_iov *iov);
void intel_iov_vf_put_wakeref_wa(struct intel_iov *iov);

#endif /* __INTEL_IOV_H__ */
