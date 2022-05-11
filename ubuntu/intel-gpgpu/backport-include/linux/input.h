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
#ifndef __BACKPORT_INPUT_H
#define __BACKPORT_INPUT_H
#include_next <linux/input.h>

#ifndef KEY_WIMAX
#define KEY_WIMAX		246
#endif

#ifndef KEY_WPS_BUTTON
#define KEY_WPS_BUTTON		0x211
#endif

#ifndef KEY_RFKILL
#define KEY_RFKILL		247
#endif

#ifndef SW_RFKILL_ALL
#define SW_RFKILL_ALL           0x03
#endif

#endif /* __BACKPORT_INPUT_H */
