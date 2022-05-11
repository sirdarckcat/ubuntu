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
#ifndef __BACKPORT_ASM_ATOMIC_H
#define __BACKPORT_ASM_ATOMIC_H
#include_next <asm/atomic.h>
#include <linux/version.h>
#include <asm/barrier.h>

#if LINUX_VERSION_IS_LESS(3,1,0)
/*
 * In many versions, several architectures do not seem to include an
 * atomic64_t implementation, and do not include the software emulation from
 * asm-generic/atomic64_t.
 * Detect and handle this here.
 */
#if (!defined(ATOMIC64_INIT) && !defined(CONFIG_X86) && !(defined(CONFIG_ARM) && !defined(CONFIG_GENERIC_ATOMIC64)))
#include <asm-generic/atomic64.h>
#endif
#endif

#endif /* __BACKPORT_ASM_ATOMIC_H */
