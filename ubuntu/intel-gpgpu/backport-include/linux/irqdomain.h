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
#ifndef __BACKPORT_LINUX_IRQDOMAIN_H
#define __BACKPORT_LINUX_IRQDOMAIN_H
#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(3,1,0)
#include_next <linux/irqdomain.h>
#endif

#endif /* __BACKPORT_LINUX_IRQDOMAIN_H */
