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
#ifndef _BACKPORT_LINUX_NOSPEC_H
#define _BACKPORT_LINUX_NOSPEC_H

#if LINUX_VERSION_IS_GEQ(4,15,2) || \
    LINUX_VERSION_IN_RANGE(4,14,18, 4,15,0) || \
    LINUX_VERSION_IN_RANGE(4,9,81, 4,10,0) || \
    LINUX_VERSION_IN_RANGE(4,4,118, 4,5,0)
#include_next <linux/nospec.h>
#else
#define array_index_nospec(index, size)	(index)
#endif

#endif /* _BACKPORT_LINUX_NOSPEC_H */
