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
#define DRM_XE_EUDEBUG_IOCTL_READ_EVENT		_IO('j', 0x0)
#define DRM_XE_EUDEBUG_IOCTL_EU_CONTROL		_IOWR('j', 0x2, struct drm_xe_eudebug_eu_control)

/* XXX: Document events to match their internal counterparts when moved to xe_drm.h */
struct drm_xe_eudebug_event {
	__u32 len;

	__u16 type;
#define DRM_XE_EUDEBUG_EVENT_NONE		0
#define DRM_XE_EUDEBUG_EVENT_READ		1
#define DRM_XE_EUDEBUG_EVENT_OPEN		2
#define DRM_XE_EUDEBUG_EVENT_VM			3
#define DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE		4
#define DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE_PLACEMENTS 5
#define DRM_XE_EUDEBUG_EVENT_EU_ATTENTION	6

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

struct drm_xe_eudebug_event_exec_queue_placements {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 vm_handle;
	__u64 exec_queue_handle;
	__u64 lrc_handle;
	__u32 num_placements;
	__u32 pad;
	/**
	 * @instances: user pointer to num_placements sized array of struct
	 * drm_xe_engine_class_instance
	 */
	__u64 instances[];
};

struct drm_xe_eudebug_event_eu_attention {
	struct drm_xe_eudebug_event base;

	__u64 client_handle;
	__u64 exec_queue_handle;
	__u64 lrc_handle;
	__u32 flags;
	__u32 bitmask_size;
	__u8 bitmask[];
};

struct drm_xe_eudebug_eu_control {
	__u64 client_handle;

#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_INTERRUPT_ALL	0
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_STOPPED		1
#define DRM_XE_EUDEBUG_EU_CONTROL_CMD_RESUME		2
	__u32 cmd;
	__u32 flags;

	__u64 seqno;

	__u64 exec_queue_handle;
	__u64 lrc_handle;
	__u32 reserved;
	__u32 bitmask_size;
	__u64 bitmask_ptr;
};

#if defined(__cplusplus)
}
#endif

#endif
