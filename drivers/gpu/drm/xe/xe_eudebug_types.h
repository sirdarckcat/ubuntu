/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef __XE_EUDEBUG_TYPES_H_

#include <linux/completion.h>
#include <linux/kfifo.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/rhashtable.h>
#include <linux/wait.h>
#include <linux/xarray.h>

#include <uapi/drm/xe_drm.h>

struct xe_device;
struct task_struct;
struct xe_eudebug;
struct xe_eudebug_event;
struct xe_hw_engine;
struct workqueue_struct;
struct xe_exec_queue;
struct xe_lrc;

#define CONFIG_DRM_XE_DEBUGGER_EVENT_QUEUE_SIZE 64

/**
 * struct xe_eudebug_handle - eudebug resource handle
 */
struct xe_eudebug_handle {
	/** @key: key value in rhashtable <key:id> */
	u64 key;

	/** @id: opaque handle id for xarray <id:key> */
	int id;

	/** @rh_head: rhashtable head */
	struct rhash_head rh_head;
};

/**
 * struct xe_eudebug_resource - Resource map for one resource
 */
struct xe_eudebug_resource {
	/** @xa: xarrays for <id->key> */
	struct xarray xa;

	/** @rh rhashtable for <key->id> */
	struct rhashtable rh;
};

#define XE_EUDEBUG_RES_TYPE_CLIENT	0
#define XE_EUDEBUG_RES_TYPE_VM		1
#define XE_EUDEBUG_RES_TYPE_EXEC_QUEUE	2
#define XE_EUDEBUG_RES_TYPE_LRC		3
#define XE_EUDEBUG_RES_TYPE_COUNT	(XE_EUDEBUG_RES_TYPE_LRC + 1)

/**
 * struct xe_eudebug_resources - eudebug resources for all types
 */
struct xe_eudebug_resources {
	/** @lock: guards access into rt */
	struct mutex lock;

	/** @rt: resource maps for all types */
	struct xe_eudebug_resource rt[XE_EUDEBUG_RES_TYPE_COUNT];
};

/**
 * struct xe_eudebug_eu_control_ops - interface for eu thread
 * state control backend
 */
struct xe_eudebug_eu_control_ops {
	/** @interrupt_all: interrupts workload active on given hwe */
	int (*interrupt_all)(struct xe_eudebug *e, struct xe_exec_queue *q,
			     struct xe_lrc *lrc);

	/** @resume: resumes threads reflected by bitmask active on given hwe */
	int (*resume)(struct xe_eudebug *e, struct xe_exec_queue *q,
		      struct xe_lrc *lrc, u8 *bitmap, unsigned int bitmap_size);

	/** @stopped: returns bitmap reflecting threads which signal attention */
	int (*stopped)(struct xe_eudebug *e, struct xe_exec_queue *q,
		       struct xe_lrc *lrc, u8 *bitmap, unsigned int bitmap_size);
};

/**
 * struct xe_eudebug - Top level struct for eudebug: the connection
 */
struct xe_eudebug {
	/** @ref: kref counter for this struct */
	struct kref ref;

	/** @rcu: rcu_head for rcu destruction */
	struct rcu_head rcu;

	/** @connection_link: our link into the xe_device:eudebug.list */
	struct list_head connection_link;

	struct {
		/** @status: connected = 1, disconnected = error */
#define XE_EUDEBUG_STATUS_CONNECTED 1
		int status;

		/** @lock: guards access to status */
		spinlock_t lock;
	} connection;

	/** @xe: the parent device we are serving */
	struct xe_device *xe;

	/** @target_task: the task that we are debugging */
	struct task_struct *target_task;

	/** @res: the resource maps we track for target_task */
	struct xe_eudebug_resources *res;

	/** @session: session number for this connection (for logs) */
	u64 session;

	/** @discovery: completion to wait for discovery */
	struct completion discovery;

	/** @discovery_work: worker to discover resources for target_task */
	struct work_struct discovery_work;

	/** eu_lock: guards operations on eus (eu thread control and attention) */
	struct mutex eu_lock;

	/** @events: kfifo queue of to-be-delivered events */
	struct {
		/** @lock: guards access to fifo */
		spinlock_t lock;

		/** @fifo: queue of events pending */
		DECLARE_KFIFO(fifo,
			      struct xe_eudebug_event *,
			      CONFIG_DRM_XE_DEBUGGER_EVENT_QUEUE_SIZE);

		/** @write_done: waitqueue for signalling write to fifo */
		wait_queue_head_t write_done;

		/** @read_done: waitqueue for signalling read from fifo */
		wait_queue_head_t read_done;

		/** @event_seqno: seqno counter to stamp events for fifo */
		atomic_long_t seqno;
	} events;

	/** @ops operations for eu_control */
	struct xe_eudebug_eu_control_ops *ops;
};

/**
 * struct xe_eudebug_event - Internal base event struct for eudebug
 */
struct xe_eudebug_event {
	/** @len: length of this event, including payload */
	u32 len;

	/** @type: message type */
	u16 type;

	/** @flags: message flags */
	u16 flags;

	/** @seqno: sequence number for ordering */
	u64 seqno;

	/** @reserved: reserved field MBZ */
	u64 reserved;

	/** @data: payload bytes */
	u8 data[];
};

/**
 * struct xe_eudebug_event_open - Internal event for client open/close
 */
struct xe_eudebug_event_open {
	/** @base: base event */
	struct xe_eudebug_event base;

	/** @client_handle: opaque handle for client */
	u64 client_handle;
};

/**
 * struct xe_eudebug_event_vm - Internal event for vm open/close
 */
struct xe_eudebug_event_vm {
	/** @base: base event */
	struct xe_eudebug_event base;

	/** @client_handle: client containing the vm open/close */
	u64 client_handle;

	/** @vm_handle: vm handle it's open/close */
	u64 vm_handle;
};

/**
 * struct xe_eudebug_event_exec_queue - Internal event for
 * exec_queue create/destroy
 */
struct xe_eudebug_event_exec_queue {
	/** @base: base event */
	struct xe_eudebug_event base;

	/** @client_handle: client for the engine create/destroy */
	u64 client_handle;

	/** @vm_handle: vm handle for the engine create/destroy */
	u64 vm_handle;

	/** @exec_queue_handle: engine handle */
	u64 exec_queue_handle;

	/** @engine_handle: engine class */
	u32 engine_class;

	/** @width: submission width (number BB per exec) for this exec queue */
	u32 width;

	/** @lrc_handles: handles for each logical ring context created with this exec queue */
	u64 lrc_handle[] __counted_by(width);
};

struct xe_eudebug_event_exec_queue_placements {
	/** @base: base event */
	struct xe_eudebug_event base;

	/** @client_handle: client for the engine create/destroy */
	u64 client_handle;

	/** @vm_handle: vm handle for the engine create/destroy */
	u64 vm_handle;

	/** @exec_queue_handle: engine handle */
	u64 exec_queue_handle;

	/** @engine_handle: engine class */
	u64 lrc_handle;

	/** @num_placements: all possible placements for given lrc */
	u32 num_placements;

	/** @pad: padding */
	u32 pad;

	/** @instances: num_placements sized array containing drm_xe_engine_class_instance*/
	u64 instances[]; __counted_by(num_placements);
};

/**
 * struct xe_eudebug_event_eu_attention - Internal event for EU attention
 */
struct xe_eudebug_event_eu_attention {
	/** @base: base event */
	struct xe_eudebug_event base;

	/** @client_handle: client for the attention */
	u64 client_handle;

	/** @exec_queue_handle: handle of exec_queue which raised attention */
	u64 exec_queue_handle;

	/** @lrc_handle: lrc handle of the workload which raised attention */
	u64 lrc_handle;

	/** @flags: eu attention event flags, currently MBZ */
	u32 flags;

	/** @bitmask_size: size of the bitmask, specific to device */
	u32 bitmask_size;

	/**
	 * @bitmask: reflects threads currently signalling attention,
	 * starting from natural hardware order of DSS=0, eu=0
	 */
	u8 bitmask[] __counted_by(bitmask_size);
};

#endif
