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
 * Copyright © 2019 Intel Corporation
 */

#ifndef __INTEL_ACPI_H__
#define __INTEL_ACPI_H__

struct drm_i915_private;

#ifdef CONFIG_ACPI
void intel_register_dsm_handler(void);
void intel_unregister_dsm_handler(void);
void intel_dsm_get_bios_data_funcs_supported(struct drm_i915_private *i915);
void intel_acpi_device_id_update(struct drm_i915_private *i915);
#else
static inline void intel_register_dsm_handler(void) { return; }
static inline void intel_unregister_dsm_handler(void) { return; }
static inline
void intel_dsm_get_bios_data_funcs_supported(struct drm_i915_private *i915) { return; }
static inline
void intel_acpi_device_id_update(struct drm_i915_private *i915) { return; }
#endif /* CONFIG_ACPI */

#endif /* __INTEL_ACPI_H__ */
