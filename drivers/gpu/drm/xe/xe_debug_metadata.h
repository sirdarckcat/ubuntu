/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _XE_DEBUG_METADATA_H_
#define _XE_DEBUG_METADATA_H_

struct drm_device;
struct drm_file;

#if IS_ENABLED(CONFIG_DRM_XE_EUDEBUG)

#include "xe_debug_metadata_types.h"

void xe_debug_metadata_put(struct xe_debug_metadata *mdata);

int xe_debug_metadata_create_ioctl(struct drm_device *dev,
				   void *data,
				   struct drm_file *file);

int xe_debug_metadata_destroy_ioctl(struct drm_device *dev,
				    void *data,
				    struct drm_file *file);
#else /* CONFIG_DRM_XE_EUDEBUG */

#include <linux/errno.h>

struct xe_debug_metadata;

static inline void xe_debug_metadata_put(struct xe_debug_metadata *mdata) { }

static inline int xe_debug_metadata_create_ioctl(struct drm_device *dev,
						 void *data,
						 struct drm_file *file)
{
	return -EOPNOTSUPP;
}

static inline int xe_debug_metadata_destroy_ioctl(struct drm_device *dev,
						  void *data,
						  struct drm_file *file)
{
	return -EOPNOTSUPP;
}

#endif /* CONFIG_DRM_XE_EUDEBUG */


#endif
