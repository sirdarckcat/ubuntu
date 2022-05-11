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
 * Copyright © 2021 Intel Corporation
 */

#ifndef __I915_IOSF_MBI_H__
#define __I915_IOSF_MBI_H__

#if IS_ENABLED(CONFIG_IOSF_MBI)
#include <asm/iosf_mbi.h>
#else

/* Stubs to compile for all non-x86 archs */
#define MBI_PMIC_BUS_ACCESS_BEGIN       1
#define MBI_PMIC_BUS_ACCESS_END         2

struct notifier_block;

static inline void iosf_mbi_punit_acquire(void) {}
static inline void iosf_mbi_punit_release(void) {}
static inline void iosf_mbi_assert_punit_acquired(void) {}

static inline
int iosf_mbi_register_pmic_bus_access_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int
iosf_mbi_unregister_pmic_bus_access_notifier_unlocked(struct notifier_block *nb)
{
	return 0;
}

static inline
int iosf_mbi_unregister_pmic_bus_access_notifier(struct notifier_block *nb)
{
	return 0;
}
#endif

#endif /* __I915_IOSF_MBI_H__ */
