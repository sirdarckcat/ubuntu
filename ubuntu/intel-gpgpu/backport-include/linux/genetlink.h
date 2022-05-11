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
#ifndef __BACKPORT_LINUX_GENETLINK_H
#define __BACKPORT_LINUX_GENETLINK_H
#include_next <linux/genetlink.h>

/* This backports:
 *
 * commit e9412c37082b5c932e83364aaed0c38c2ce33acb
 * Author: Neil Horman <nhorman@tuxdriver.com>
 * Date:   Tue May 29 09:30:41 2012 +0000
 *
 *     genetlink: Build a generic netlink family module alias
 */
#ifndef MODULE_ALIAS_GENL_FAMILY
#define MODULE_ALIAS_GENL_FAMILY(family)\
 MODULE_ALIAS_NET_PF_PROTO_NAME(PF_NETLINK, NETLINK_GENERIC, "-family-" family)
#endif

#endif /* __BACKPORT_LINUX_GENETLINK_H */
