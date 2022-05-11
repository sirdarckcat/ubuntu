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

#ifndef __I915_SRIOV_H__
#define __I915_SRIOV_H__

#include "i915_drv.h"
#include "i915_virtualization.h"

struct drm_i915_private;
struct drm_printer;

#ifdef CONFIG_PCI_IOV
#define IS_SRIOV_PF(i915) (IOV_MODE(i915) == I915_IOV_MODE_SRIOV_PF)
#else
#define IS_SRIOV_PF(i915) false
#endif
#define IS_SRIOV_VF(i915) (IOV_MODE(i915) == I915_IOV_MODE_SRIOV_VF)

#define IS_SRIOV(i915) (IS_SRIOV_PF(i915) || IS_SRIOV_VF(i915))

enum i915_iov_mode i915_sriov_probe(struct drm_i915_private *i915);
int i915_sriov_early_tweaks(struct drm_i915_private *i915);
void i915_sriov_print_info(struct drm_i915_private *i915, struct drm_printer *p);

/* PF only */
void i915_sriov_pf_confirm(struct drm_i915_private *i915);
void i915_sriov_pf_abort(struct drm_i915_private *i915, int err);
bool i915_sriov_pf_aborted(struct drm_i915_private *i915);
int i915_sriov_pf_status(struct drm_i915_private *i915);
int i915_sriov_pf_get_device_totalvfs(struct drm_i915_private *i915);
int i915_sriov_pf_get_totalvfs(struct drm_i915_private *i915);
int i915_sriov_pf_enable_vfs(struct drm_i915_private *i915, int numvfs);
int i915_sriov_pf_disable_vfs(struct drm_i915_private *i915);
int i915_sriov_pf_stop_vf(struct drm_i915_private *i915, unsigned int vfid);
int i915_sriov_pf_pause_vf(struct drm_i915_private *i915, unsigned int vfid);
int i915_sriov_pf_resume_vf(struct drm_i915_private *i915, unsigned int vfid);
int i915_sriov_pf_clear_vf(struct drm_i915_private *i915, unsigned int vfid);

bool i915_sriov_pf_is_auto_provisioning_enabled(struct drm_i915_private *i915);
int i915_sriov_pf_set_auto_provisioning(struct drm_i915_private *i915, bool enable);

/* VF only */
void i915_sriov_vf_start_migration_recovery(struct drm_i915_private *i915);

#endif /* __I915_SRIOV_H__ */
