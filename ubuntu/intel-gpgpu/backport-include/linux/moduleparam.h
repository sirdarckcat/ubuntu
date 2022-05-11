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
#ifndef __BACKPORT_LINUX_MODULEPARAM_H
#define __BACKPORT_LINUX_MODULEPARAM_H
#include_next <linux/moduleparam.h>

#if LINUX_VERSION_IS_LESS(4,2,0)
#define kernel_param_lock LINUX_I915_BACKPORT(kernel_param_lock)
static inline void kernel_param_lock(struct module *mod)
{
	__kernel_param_lock();
}
#define kernel_param_unlock LINUX_I915_BACKPORT(kernel_param_unlock)
static inline void kernel_param_unlock(struct module *mod)
{
	__kernel_param_unlock();
}
#endif

#if LINUX_VERSION_IS_LESS(3,8,0)
#undef __MODULE_INFO
#ifdef MODULE
#define __MODULE_INFO(tag, name, info)					  \
static const char __UNIQUE_ID(name)[]					  \
  __used __attribute__((section(".modinfo"), unused, aligned(1)))	  \
  = __stringify(tag) "=" info
#else  /* !MODULE */
/* This struct is here for syntactic coherency, it is not used */
#define __MODULE_INFO(tag, name, info)					  \
  struct __UNIQUE_ID(name) {}
#endif
#endif /* < 3.8 */

#if LINUX_VERSION_IS_LESS(3,17,0)
extern struct kernel_param_ops param_ops_ullong;
extern int param_set_ullong(const char *val, const struct kernel_param *kp);
extern int param_get_ullong(char *buffer, const struct kernel_param *kp);
#define param_check_ullong(name, p) __param_check(name, p, unsigned long long)
#endif

#ifndef module_param_hw_array
#define module_param_hw_array(name, type, hwtype, nump, perm) \
	module_param_array(name, type, nump, perm)
#endif

#endif /* __BACKPORT_LINUX_MODULEPARAM_H */
