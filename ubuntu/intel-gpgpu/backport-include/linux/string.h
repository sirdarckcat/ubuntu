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
#ifndef __BACKPORT_LINUX_STRING_H
#define __BACKPORT_LINUX_STRING_H
#include_next <linux/string.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,5,0)
#define memdup_user_nul LINUX_I915_BACKPORT(memdup_user_nul)
extern void *memdup_user_nul(const void __user *, size_t);
#endif

/* this was added in v3.2.65, v3.4.106, v3.10.60, v3.12.33, v3.14.24,
 * v3.17.3 and v3.18 */
#if !(LINUX_VERSION_IS_GEQ(3,17,3) || \
      (LINUX_VERSION_IS_GEQ(3,14,24) && \
      LINUX_VERSION_IS_LESS(3,15,0)) || \
      (LINUX_VERSION_IS_GEQ(3,12,33) && \
      LINUX_VERSION_IS_LESS(3,13,0)) || \
      (LINUX_VERSION_IS_GEQ(3,10,60) && \
      LINUX_VERSION_IS_LESS(3,11,0)) || \
      (LINUX_VERSION_IS_GEQ(3,4,106) && \
      LINUX_VERSION_IS_LESS(3,5,0)) || \
      (LINUX_VERSION_IS_GEQ(3,2,65) && \
      LINUX_VERSION_IS_LESS(3,3,0)))
#define memzero_explicit LINUX_I915_BACKPORT(memzero_explicit)
void memzero_explicit(void *s, size_t count);
#endif

#if LINUX_VERSION_IS_LESS(4,3,0)
ssize_t strscpy(char *dest, const char *src, size_t count);
#endif

#if LINUX_VERSION_IS_LESS(4,2,0)
char *strreplace(char *s, char old, char new);
#endif

#if LINUX_VERSION_IS_LESS(4,6,0)
int match_string(const char * const *array, size_t n, const char *string);
#endif /* LINUX_VERSION_IS_LESS(4,5,0) */

#endif /* __BACKPORT_LINUX_STRING_H */
