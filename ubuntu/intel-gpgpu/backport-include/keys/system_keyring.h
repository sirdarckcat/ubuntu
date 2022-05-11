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
#ifndef __BP_SYSTEM_KEYRING_H
#define __BP_SYSTEM_KEYRING_H
#ifndef CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION
#include_next <keys/system_keyring.h>
#else
#include <linux/key.h>

#define is_hash_blacklisted(...)	0
#endif /* CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION */
#endif /* __BP_SYSTEM_KEYRING_H */
