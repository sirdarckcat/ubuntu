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
 * Copyright © 2020 Intel Corporation
 */

#ifndef __INTEL_DISPLAY_DEBUGFS_H__
#define __INTEL_DISPLAY_DEBUGFS_H__

#include <linux/types.h>

struct drm_crtc;
struct drm_i915_private;
struct intel_connector;

#ifdef CONFIG_DEBUG_FS
void intel_display_debugfs_register(struct drm_i915_private *i915);
void intel_connector_debugfs_add(struct intel_connector *connector);
void intel_crtc_debugfs_add(struct drm_crtc *crtc);
bool intel_connector_need_suppress_wakeup_hpd(struct intel_connector *connector);
#else
static inline void intel_display_debugfs_register(struct drm_i915_private *i915) {}
static inline void intel_connector_debugfs_add(struct intel_connector *connector) {}
static inline void intel_crtc_debugfs_add(struct drm_crtc *crtc) {}
static inline bool intel_connector_need_suppress_wakeup_hpd(struct intel_connector *connector)
{
	return false;
}
#endif

#endif /* __INTEL_DISPLAY_DEBUGFS_H__ */
