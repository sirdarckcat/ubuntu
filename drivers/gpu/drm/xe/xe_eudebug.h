/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _XE_EUDEBUG_H_

struct drm_device;
struct drm_file;
struct xe_device;
struct xe_file;
struct xe_vm;
struct xe_exec_queue;

#if IS_ENABLED(CONFIG_DRM_XE_EUDEBUG)

int xe_eudebug_connect_ioctl(struct drm_device *dev,
			     void *data,
			     struct drm_file *file);

void xe_eudebug_init(struct xe_device *xe);
void xe_eudebug_fini(struct xe_device *xe);

void xe_eudebug_file_open(struct xe_file *xef);
void xe_eudebug_file_close(struct xe_file *xef);

void xe_eudebug_vm_create(struct xe_file *xef, struct xe_vm *vm);
void xe_eudebug_vm_destroy(struct xe_file *xef, struct xe_vm *vm);

void xe_eudebug_exec_queue_create(struct xe_file *xef, struct xe_exec_queue *q);
void xe_eudebug_exec_queue_destroy(struct xe_file *xef, struct xe_exec_queue *q);

#else

static inline int xe_eudebug_connect_ioctl(struct drm_device *dev,
					   void *data,
					   struct drm_file *file) { return 0; }

static inline void xe_eudebug_init(struct xe_device *xe) { }
static inline void xe_eudebug_fini(struct xe_device *xe) { }

static inline void xe_eudebug_file_open(struct xe_file *xef) { }
static inline void xe_eudebug_file_close(struct xe_file *xef) { }

static inline void xe_eudebug_vm_create(struct xe_file *xef, struct xe_vm *vm) { }
static inline void xe_eudebug_vm_destroy(struct xe_file *xef, struct xe_vm *vm) { }

static inline void xe_eudebug_exec_queue_create(struct xe_file *xef, struct xe_exec_queue *q) { }
static inline void xe_eudebug_exec_queue_destroy(struct xe_file *xef, struct xe_exec_queue *q) { }

#endif /* CONFIG_DRM_XE_EUDEBUG */

#endif
