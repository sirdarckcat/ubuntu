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
#ifndef STUB_X86_ASM_CPUFEATURE_H
#define STUB_X86_ASM_CPUFEATURE_H

#include_next <asm/cpufeature.h>

/*
 * This is a lie - there is no such thing on ARM64, but this makes it take the
 * right branches
 */
#define pat_enabled()		1

#define static_cpu_has(x)	0

#endif
