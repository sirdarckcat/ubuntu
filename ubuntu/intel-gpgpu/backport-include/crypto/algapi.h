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
#ifndef __BP_ALGAPI_H
#define __BP_ALGAPI_H
#include <linux/version.h>
#include_next <crypto/algapi.h>

#if LINUX_VERSION_IS_LESS(3,13,0)
#define __crypto_memneq LINUX_I915_BACKPORT(__crypto_memneq)
noinline unsigned long __crypto_memneq(const void *a, const void *b, size_t size);
#define crypto_memneq LINUX_I915_BACKPORT(crypto_memneq)
static inline int crypto_memneq(const void *a, const void *b, size_t size)
{
        return __crypto_memneq(a, b, size) != 0UL ? 1 : 0;
}
#endif

#endif /* __BP_ALGAPI_H */
