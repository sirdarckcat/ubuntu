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
#ifndef _BACKPORT_LINUX_CRC7_H
#define _BACKPORT_LINUX_CRC7_H
#include_next <linux/crc7.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,16,0)
#define crc7_be LINUX_I915_BACKPORT(crc7_be)
static inline u8 crc7_be(u8 crc, const u8 *buffer, size_t len)
{
	return crc7(crc, buffer, len) << 1;
}
#endif /* < 3.16 */

#endif /* _BACKPORT_LINUX_CRC7_H */
