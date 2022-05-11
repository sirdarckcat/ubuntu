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
/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2020 Intel Corporation
 */

#ifndef SHMEM_UTILS_H
#define SHMEM_UTILS_H

#include <linux/types.h>

struct iosys_map;
struct drm_i915_gem_object;
struct file;

struct file *shmem_create_from_data(const char *name, void *data, size_t len);
struct file *shmem_create_from_object(struct drm_i915_gem_object *obj);

void *shmem_pin_map(struct file *file);
void shmem_unpin_map(struct file *file, void *ptr);

int shmem_read_to_iosys_map(struct file *file, loff_t off,
			    struct iosys_map *map, size_t map_off, size_t len);
int shmem_read(struct file *file, loff_t off, void *dst, size_t len);
int shmem_write(struct file *file, loff_t off, void *src, size_t len);

#endif /* SHMEM_UTILS_H */
