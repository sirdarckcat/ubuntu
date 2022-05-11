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
#ifndef __BACKPORT_LINUX_NL80211_H
#define __BACKPORT_LINUX_NL80211_H
#include_next <linux/nl80211.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,3,0)
#define NL80211_FEATURE_SK_TX_STATUS 0
#endif

#endif /* __BACKPORT_LINUX_NL80211_H */
