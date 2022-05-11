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
/*
 * Copyright(c) 2011-2016 Intel Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _INTEL_GVT_H_
#define _INTEL_GVT_H_

struct drm_i915_private;

#ifdef CPTCFG_DRM_I915_GVT
int intel_gvt_init(struct drm_i915_private *dev_priv);
void intel_gvt_driver_remove(struct drm_i915_private *dev_priv);
int intel_gvt_init_device(struct drm_i915_private *dev_priv);
void intel_gvt_clean_device(struct drm_i915_private *dev_priv);
int intel_gvt_init_host(void);
void intel_gvt_sanitize_options(struct drm_i915_private *dev_priv);
void intel_gvt_resume(struct drm_i915_private *dev_priv);
#else
static inline int intel_gvt_init(struct drm_i915_private *dev_priv)
{
	return 0;
}

static inline void intel_gvt_driver_remove(struct drm_i915_private *dev_priv)
{
}

static inline void intel_gvt_sanitize_options(struct drm_i915_private *dev_priv)
{
}

static inline void intel_gvt_resume(struct drm_i915_private *dev_priv)
{
}
#endif

#endif /* _INTEL_GVT_H_ */
