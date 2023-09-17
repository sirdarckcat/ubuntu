/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _XE_DEBUG_METADATA_H_
#define _XE_DEBUG_METADATA_H_

#include <linux/types.h>

struct drm_device;
struct drm_file;
struct xe_file;

#if IS_ENABLED(CONFIG_DRM_XE_EUDEBUG)

#include "xe_debug_metadata_types.h"
#include "xe_vm_types.h"

struct xe_debug_metadata *xe_debug_metadata_get(struct xe_file *xef, u32 id);
void xe_debug_metadata_put(struct xe_debug_metadata *mdata);

int xe_debug_metadata_create_ioctl(struct drm_device *dev,
				   void *data,
				   struct drm_file *file);

int xe_debug_metadata_destroy_ioctl(struct drm_device *dev,
				    void *data,
				    struct drm_file *file);

static inline void xe_eudebug_move_vma_metadata(struct xe_eudebug_vma_metadata *from,
						struct xe_eudebug_vma_metadata *to)
{
	list_splice_tail_init(&from->list, &to->list);
}

int xe_eudebug_copy_vma_metadata(struct xe_eudebug_vma_metadata *from,
				 struct xe_eudebug_vma_metadata *to);
void xe_eudebug_free_vma_metadata(struct xe_eudebug_vma_metadata *mdata);

int vm_bind_op_ext_attach_debug(struct xe_device *xe,
				struct xe_file *xef,
				struct drm_gpuva_ops *ops,
				u32 operation, u64 extension);

#else /* CONFIG_DRM_XE_EUDEBUG */

#include <linux/errno.h>

struct xe_debug_metadata;
struct xe_device;
struct xe_eudebug_vma_metadata;
struct drm_gpuva_ops;

static inline struct xe_debug_metadata *xe_debug_metadata_get(struct xe_file *xef, u32 id) { return NULL; }
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

static inline void xe_eudebug_move_vma_metadata(struct xe_eudebug_vma_metadata *from,
						struct xe_eudebug_vma_metadata *to)
{
}

static inline int xe_eudebug_copy_vma_metadata(struct xe_eudebug_vma_metadata *from,
					       struct xe_eudebug_vma_metadata *to)
{
	return 0;
}

static inline void xe_eudebug_free_vma_metadata(struct xe_eudebug_vma_metadata *mdata)
{
}

static inline int vm_bind_op_ext_attach_debug(struct xe_device *xe,
					      struct xe_file *xef,
					      struct drm_gpuva_ops *ops,
					      u32 operation, u64 extension)
{
	return -EINVAL;
}

#endif /* CONFIG_DRM_XE_EUDEBUG */


#endif
