// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 */
#include "xe_debug_metadata.h"

#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <uapi/drm/xe_drm.h>

#include "xe_device.h"
#include "xe_macros.h"

static void xe_debug_metadata_release(struct kref *ref)
{
	struct xe_debug_metadata *mdata = container_of(ref, struct xe_debug_metadata, refcount);

	kvfree(mdata->ptr);
	kfree(mdata);
}

void xe_debug_metadata_put(struct xe_debug_metadata *mdata)
{
	kref_put(&mdata->refcount, xe_debug_metadata_release);
}

int xe_debug_metadata_create_ioctl(struct drm_device *dev,
				   void *data,
				   struct drm_file *file)
{
	struct xe_device *xe = to_xe_device(dev);
	struct xe_file *xef = to_xe_file(file);
	struct drm_xe_debug_metadata_create *args = data;
	struct xe_debug_metadata *mdata;
	int err;
	u32 id;

	if (XE_IOCTL_DBG(xe, args->extensions))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, args->type > DRM_XE_DEBUG_METADATA_PROGRAM_MODULE))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, !args->user_addr || !args->len))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, !access_ok(u64_to_user_ptr(args->user_addr), args->len)))
		return -EFAULT;

	mdata = kzalloc(sizeof(*mdata), GFP_KERNEL);
	if (!mdata)
		return -ENOMEM;

	mdata->len = args->len;
	mdata->type = args->type;

	mdata->ptr = kvmalloc(mdata->len, GFP_KERNEL);
	if (!mdata->ptr) {
		kfree(mdata);
		return -ENOMEM;
	}
	kref_init(&mdata->refcount);

	err = copy_from_user(mdata->ptr, u64_to_user_ptr(args->user_addr), mdata->len);
	if (err) {
		err = -EFAULT;
		goto put_mdata;
	}

	mutex_lock(&xef->eudebug.metadata.lock);
	err = xa_alloc(&xef->eudebug.metadata.xa, &id, mdata, xa_limit_32b, GFP_KERNEL);
	mutex_unlock(&xef->eudebug.metadata.lock);

	if (err)
		goto put_mdata;

	args->metadata_id = id;

	return 0;

put_mdata:
	xe_debug_metadata_put(mdata);
	return err;
}

int xe_debug_metadata_destroy_ioctl(struct drm_device *dev,
				    void *data,
				    struct drm_file *file)
{
	struct xe_device *xe = to_xe_device(dev);
	struct xe_file *xef = to_xe_file(file);
	struct drm_xe_debug_metadata_destroy * const args = data;
	struct xe_debug_metadata *mdata;

	if (XE_IOCTL_DBG(xe, args->extensions))
		return -EINVAL;

	mutex_lock(&xef->eudebug.metadata.lock);
	mdata = xa_erase(&xef->eudebug.metadata.xa, args->metadata_id);
	mutex_unlock(&xef->eudebug.metadata.lock);
	if (XE_IOCTL_DBG(xe, !mdata))
		return -ENOENT;

	xe_debug_metadata_put(mdata);

	return 0;
}
