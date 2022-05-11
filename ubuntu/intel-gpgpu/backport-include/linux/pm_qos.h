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
#ifndef _COMPAT_LINUX_PM_QOS_H
#define _COMPAT_LINUX_PM_QOS_H 1

#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(3,2,0)
#include_next <linux/pm_qos.h>
#else
#include <linux/pm_qos_params.h>
#endif /* LINUX_VERSION_IS_GEQ(3,2,0) */

#ifndef PM_QOS_DEFAULT_VALUE
#define PM_QOS_DEFAULT_VALUE -1
#endif

#endif	/* _COMPAT_LINUX_PM_QOS_H */
