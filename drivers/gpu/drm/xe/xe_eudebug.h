/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _XE_EUDEBUG_H_

#include <linux/types.h>

struct drm_device;
struct drm_file;
struct xe_device;
struct xe_file;
struct xe_vm;
struct xe_vma;
struct xe_exec_queue;
struct xe_hw_engine;
struct xe_user_fence;
struct xe_debug_metadata;
struct drm_gpuva_ops;

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

void xe_eudebug_vm_init(struct xe_vm *vm);
void xe_eudebug_vm_bind_start(struct xe_vm *vm);
void xe_eudebug_vm_bind_op_add(struct xe_vm *vm, u32 op, u64 addr, u64 range,
			       struct drm_gpuva_ops *ops);
void xe_eudebug_vm_bind_end(struct xe_vm *vm, bool has_ufence, int err);

int xe_eudebug_vm_bind_ufence(struct xe_user_fence *ufence);
void xe_eudebug_ufence_init(struct xe_user_fence *ufence, struct xe_file *xef, struct xe_vm *vm);
void xe_eudebug_ufence_fini(struct xe_user_fence *ufence);

struct xe_eudebug *xe_eudebug_get(struct xe_file *xef);
void xe_eudebug_put(struct xe_eudebug *d);

void xe_eudebug_debug_metadata_create(struct xe_file *xef, struct xe_debug_metadata *m);
void xe_eudebug_debug_metadata_destroy(struct xe_file *xef, struct xe_debug_metadata *m);

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

static inline void xe_eudebug_vm_init(struct xe_vm *vm) { }
static inline void xe_eudebug_vm_bind_start(struct xe_vm *vm) { }
static inline void xe_eudebug_vm_bind_op_add(struct xe_vm *vm, u32 op, u64 addr, u64 range, struct drm_gpuva_ops *ops) { }
static inline void xe_eudebug_vm_bind_end(struct xe_vm *vm, bool has_ufence, int err) { }

static inline int xe_eudebug_vm_bind_ufence(struct xe_user_fence *ufence) { return 0; }
static inline void xe_eudebug_ufence_init(struct xe_user_fence *ufence, struct xe_file *xef, struct xe_vm *vm) { }
static inline void xe_eudebug_ufence_fini(struct xe_user_fence *ufence) { }

static inline struct xe_eudebug *xe_eudebug_get(struct xe_file *xef) { return NULL; }
static inline void xe_eudebug_put(struct xe_eudebug *d) { }

static inline void xe_eudebug_debug_metadata_create(struct xe_file *xef, struct xe_debug_metadata *m) { }
static inline void xe_eudebug_debug_metadata_destroy(struct xe_file *xef, struct xe_debug_metadata *m) { }

#endif /* CONFIG_DRM_XE_EUDEBUG */

#endif
