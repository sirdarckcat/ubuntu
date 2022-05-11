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
#ifndef __BACKPORT_LINUX_ETHTOOL_H
#define __BACKPORT_LINUX_ETHTOOL_H
#include_next <linux/ethtool.h>
#include <linux/version.h>

#ifndef SPEED_UNKNOWN
#define SPEED_UNKNOWN  -1
#endif /* SPEED_UNKNOWN */

#ifndef DUPLEX_UNKNOWN
#define DUPLEX_UNKNOWN 0xff
#endif /* DUPLEX_UNKNOWN */

#ifndef ETHTOOL_FWVERS_LEN
#define ETHTOOL_FWVERS_LEN 32
#endif

#endif /* __BACKPORT_LINUX_ETHTOOL_H */
