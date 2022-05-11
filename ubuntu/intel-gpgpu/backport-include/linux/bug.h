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
#ifndef __BP_BUG_H
#define __BP_BUG_H
#include_next <linux/bug.h>

#ifndef __BUILD_BUG_ON_NOT_POWER_OF_2
#ifdef __CHECKER__
#define __BUILD_BUG_ON_NOT_POWER_OF_2(n) (0)
#else
#define __BUILD_BUG_ON_NOT_POWER_OF_2(n)	\
	BUILD_BUG_ON(((n) & ((n) - 1)) != 0)
#endif /* __CHECKER__ */
#endif /* __BUILD_BUG_ON_NOT_POWER_OF_2 */

#ifndef BUILD_BUG_ON_MSG
#define BUILD_BUG_ON_MSG(x, msg) BUILD_BUG_ON(x)
#endif

#endif /* __BP_BUG_H */
