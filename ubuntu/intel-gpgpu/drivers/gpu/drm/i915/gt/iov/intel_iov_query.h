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

#ifndef __INTEL_IOV_QUERY_H__
#define __INTEL_IOV_QUERY_H__

#include <linux/types.h>

struct drm_printer;
struct intel_iov;

int intel_iov_query_bootstrap(struct intel_iov *iov);
int intel_iov_query_config(struct intel_iov *iov);
int intel_iov_query_version(struct intel_iov *iov);
int intel_iov_query_runtime(struct intel_iov *iov, bool early);
void intel_iov_query_fini(struct intel_iov *iov);

void intel_iov_query_print_config(struct intel_iov *iov, struct drm_printer *p);

#endif /* __INTEL_IOV_QUERY_H__ */
