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

#ifndef __INTEL_IOV_PROVISIONING_H__
#define __INTEL_IOV_PROVISIONING_H__

#include <linux/types.h>
#include "intel_iov_types.h"

struct drm_printer;
struct intel_iov;

void intel_iov_provisioning_init_early(struct intel_iov *iov);
void intel_iov_provisioning_release(struct intel_iov *iov);
void intel_iov_provisioning_init(struct intel_iov *iov);
void intel_iov_provisioning_fini(struct intel_iov *iov);

int intel_iov_provisioning_set_sched_if_idle(struct intel_iov *iov, bool enable);
bool intel_iov_provisioning_get_sched_if_idle(struct intel_iov *iov);
int intel_iov_provisioning_set_reset_engine(struct intel_iov *iov, bool enable);
bool intel_iov_provisioning_get_reset_engine(struct intel_iov *iov);
int intel_iov_provisioning_set_sample_period(struct intel_iov *iov, u32 value);
u32 intel_iov_provisioning_get_sample_period(struct intel_iov *iov);

void intel_iov_provisioning_restart(struct intel_iov *iov);
int intel_iov_provisioning_auto(struct intel_iov *iov, unsigned int num_vfs);
int intel_iov_provisioning_verify(struct intel_iov *iov, unsigned int num_vfs);
int intel_iov_provisioning_push(struct intel_iov *iov, unsigned int num);

int intel_iov_provisioning_set_ggtt(struct intel_iov *iov, unsigned int id, u64 size);
u64 intel_iov_provisioning_get_ggtt(struct intel_iov *iov, unsigned int id);
u64 intel_iov_provisioning_query_free_ggtt(struct intel_iov *iov);
u64 intel_iov_provisioning_query_max_ggtt(struct intel_iov *iov);

int intel_iov_provisioning_set_ctxs(struct intel_iov *iov, unsigned int id, u16 num_ctxs);
u16 intel_iov_provisioning_get_ctxs(struct intel_iov *iov, unsigned int id);
u16 intel_iov_provisioning_query_max_ctxs(struct intel_iov *iov);
u16 intel_iov_provisioning_query_free_ctxs(struct intel_iov *iov);

int intel_iov_provisioning_set_dbs(struct intel_iov *iov, unsigned int id, u16 num_dbs);
u16 intel_iov_provisioning_get_dbs(struct intel_iov *iov, unsigned int id);
u16 intel_iov_provisioning_query_free_dbs(struct intel_iov *iov);
u16 intel_iov_provisioning_query_max_dbs(struct intel_iov *iov);

int intel_iov_provisioning_set_exec_quantum(struct intel_iov *iov, unsigned int id, u32 exec_quantum);
u32 intel_iov_provisioning_get_exec_quantum(struct intel_iov *iov, unsigned int id);

int intel_iov_provisioning_set_preempt_timeout(struct intel_iov *iov, unsigned int id, u32 preempt_timeout);
u32 intel_iov_provisioning_get_preempt_timeout(struct intel_iov *iov, unsigned int id);

int intel_iov_provisioning_set_lmem(struct intel_iov *iov, unsigned int id, u64 size);
u64 intel_iov_provisioning_get_lmem(struct intel_iov *iov, unsigned int id);
u64 intel_iov_provisioning_query_free_lmem(struct intel_iov *iov);
u64 intel_iov_provisioning_query_max_lmem(struct intel_iov *iov);

int intel_iov_provisioning_set_threshold(struct intel_iov *iov, unsigned int id,
					 enum intel_iov_threshold threshold, u32 value);
u32 intel_iov_provisioning_get_threshold(struct intel_iov *iov, unsigned int id,
					 enum intel_iov_threshold threshold);

int intel_iov_provisioning_clear(struct intel_iov *iov, unsigned int id);

int intel_iov_provisioning_print_ggtt(struct intel_iov *iov, struct drm_printer *p);
int intel_iov_provisioning_print_ctxs(struct intel_iov *iov, struct drm_printer *p);
int intel_iov_provisioning_print_dbs(struct intel_iov *iov, struct drm_printer *p);

#if IS_ENABLED(CPTCFG_DRM_I915_DEBUG_IOV)
int intel_iov_provisioning_move_ggtt(struct intel_iov *iov, unsigned int id);
#endif /* CPTCFG_DRM_I915_DEBUG_IOV */

#if IS_ENABLED(CPTCFG_DRM_I915_SELFTEST)
int intel_iov_provisioning_force_vgt_mode(struct intel_iov *iov);
#endif /* CPTCFG_DRM_I915_SELFTEST */

#endif /* __INTEL_IOV_PROVISIONING_H__ */
