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

#ifndef __INTEL_IOV_EVENT_H__
#define __INTEL_IOV_EVENT_H__

#include <linux/types.h>

struct drm_printer;
struct intel_iov;

void intel_iov_event_reset(struct intel_iov *iov, u32 vfid);
int intel_iov_event_process_guc2pf(struct intel_iov *iov, const u32 *msg, u32 len);
int intel_iov_event_print_events(struct intel_iov *iov, struct drm_printer *p);

#endif /* __INTEL_IOV_EVENT_H__ */
