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
#ifndef STUB_X86_ASM_CACHEFLUSH_H
#define STUB_X86_ASM_CACHEFLUSH_H 1

#include_next <asm/cacheflush.h>

#define clflush(x) do { (void)x; } while (0)
#define clflushopt(x) do { (void)x; } while (0)

/*
 * stub already added by arch:
 *
 * 	define clflush_cache_range(x, y) do {  } while (0)
 */

#endif
