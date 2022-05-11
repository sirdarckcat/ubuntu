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
#ifndef _COMPAT_LINUX_EXPORT_H
#define _COMPAT_LINUX_EXPORT_H 1

#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(3,2,0)
#include_next <linux/export.h>
#else
#ifndef pr_fmt
#define backport_undef_pr_fmt
#endif
#include <linux/module.h>
#ifdef backport_undef_pr_fmt
#undef pr_fmt
#undef backport_undef_pr_fmt
#endif
#endif /* LINUX_VERSION_IS_GEQ(3,2,0) */

#endif	/* _COMPAT_LINUX_EXPORT_H */
