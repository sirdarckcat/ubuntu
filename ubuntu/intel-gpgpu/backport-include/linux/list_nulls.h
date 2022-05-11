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
#ifndef __BACKPORT_LIST_NULLS
#define __BACKPORT_LIST_NULLS
#include_next <linux/list_nulls.h>

#ifndef NULLS_MARKER
#define NULLS_MARKER(value) (1UL | (((long)value) << 1))
#endif

#endif /* __BACKPORT_LIST_NULLS */
