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

#ifndef __I915_CMD_PARSER_H__
#define __I915_CMD_PARSER_H__

#include <linux/types.h>

struct drm_i915_private;
struct intel_engine_cs;
struct i915_vma;

int i915_cmd_parser_get_version(struct drm_i915_private *dev_priv);
int intel_engine_init_cmd_parser(struct intel_engine_cs *engine);
void intel_engine_cleanup_cmd_parser(struct intel_engine_cs *engine);
unsigned long *intel_engine_cmd_parser_alloc_jump_whitelist(u32 batch_length,
							    bool trampoline);
int intel_engine_cmd_parser(struct intel_engine_cs *engine,
			    struct i915_vma *batch,
			    unsigned long batch_offset,
			    unsigned long batch_length,
			    struct i915_vma *shadow,
			    unsigned long *jump_whitelist,
			    void *shadow_map,
			    const void *batch_map);
#define I915_CMD_PARSER_TRAMPOLINE_SIZE 8

#endif /* __I915_CMD_PARSER_H__ */
