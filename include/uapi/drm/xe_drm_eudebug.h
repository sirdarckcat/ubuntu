/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _UAPI_XE_DRM_EUDEBUG_H_
#define _UAPI_XE_DRM_EUDEBUG_H_

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Do a eudebug event read for a debugger connection.
 *
 * This ioctl is available in debug version 1.
 */
#define DRM_XE_EUDEBUG_IOCTL_READ_EVENT _IO('j', 0x0)

/* XXX: Document events to match their internal counterparts when moved to xe_drm.h */
struct drm_xe_eudebug_event {
	__u32 len;

	__u16 type;
#define DRM_XE_EUDEBUG_EVENT_NONE		0
#define DRM_XE_EUDEBUG_EVENT_READ		1
#define DRM_XE_EUDEBUG_EVENT_OPEN		2
#define DRM_XE_EUDEBUG_EVENT_VM			3
#define DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE		4

	__u16 flags;
#define DRM_XE_EUDEBUG_EVENT_CREATE		(1 << 0)
#define DRM_XE_EUDEBUG_EVENT_DESTROY		(1 << 1)
#define DRM_XE_EUDEBUG_EVENT_STATE_CHANGE	(1 << 2)
#define DRM_XE_EUDEBUG_EVENT_NEED_ACK		(1 << 3)
	__u64 seqno;
	__u64 reserved;
};

struct drm_xe_eudebug_event_client {
	struct drm_xe_eudebug_event base;

	__u64 client_handle; /* This is unique per debug connection */
};

struct drm_xe_eudebug_event_vm {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;
};

struct drm_xe_eudebug_event_exec_queue {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;
	__u64 exec_queue_handle;
	__u32 engine_class;
	__u32 width;
	__u64 lrc_handle[];
};

#if defined(__cplusplus)
}
#endif

#endif
