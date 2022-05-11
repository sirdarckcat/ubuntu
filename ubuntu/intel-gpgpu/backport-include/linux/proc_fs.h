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
#ifndef __BACKPORT_PROC_FS_H
#define __BACKPORT_PROC_FS_H
#include_next <linux/proc_fs.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,10,0)

#ifdef CONFIG_PROC_FS
/*
 * backport of:
 * procfs: new helper - PDE_DATA(inode)
 */
#define PDE_DATA LINUX_I915_BACKPORT(PDE_DATA)
static inline void *PDE_DATA(const struct inode *inode)
{
	return PROC_I(inode)->pde->data;
}
extern void proc_set_size(struct proc_dir_entry *, loff_t);
extern void proc_set_user(struct proc_dir_entry *, kuid_t, kgid_t);
#else
#define PDE_DATA LINUX_I915_BACKPORT(PDE_DATA)
static inline void *PDE_DATA(const struct inode *inode) {BUG(); return NULL;}
static inline void proc_set_size(struct proc_dir_entry *de, loff_t size) {}
static inline void proc_set_user(struct proc_dir_entry *de, kuid_t uid, kgid_t gid) {}
#endif /* CONFIG_PROC_FS */

#endif /* LINUX_VERSION_IS_LESS(3,10,0) */

#endif /* __BACKPORT_PROC_FS_H */
