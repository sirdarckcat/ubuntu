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
 * SPDX-License-Identifier: MIT
 *
 * Copyright © 2019 Intel Corporation
 */

#ifndef I915_GEM_IOCTLS_H
#define I915_GEM_IOCTLS_H

struct drm_device;
struct drm_file;

int i915_gem_busy_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file);
int i915_gem_create_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file);
int i915_gem_execbuffer2_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file);
int i915_gem_get_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_get_tiling_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file);
int i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file);
int i915_gem_mmap_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file);
int i915_gem_mmap_offset_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_pread_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file);
int i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file);
int i915_gem_set_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file);
int i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file);
int i915_gem_set_tiling_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file);
int i915_gem_sw_finish_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file);
int i915_gem_throttle_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file);
int i915_gem_userptr_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file);
int i915_gem_wait_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file);
int i915_gem_wait_user_fence_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file);
int i915_gem_clos_reserve_ioctl(struct drm_device *dev, void *data,
                               struct drm_file *file);
int i915_gem_clos_free_ioctl(struct drm_device *dev, void *data,
                               struct drm_file *file);
int i915_gem_cache_reserve_ioctl(struct drm_device *dev, void *data,
                               struct drm_file *file);

#endif
