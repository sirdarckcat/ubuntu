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
#ifndef __BACKPORT_LINUX_FIRMWARE_H
#define __BACKPORT_LINUX_FIRMWARE_H
#include_next <linux/firmware.h>

#if LINUX_VERSION_IS_LESS(3,14,0)
#define request_firmware_direct(fw, name, device) request_firmware(fw, name, device)
#endif
#if LINUX_VERSION_IS_LESS(4,18,0)
#define firmware_request_nowarn(fw, name, device) request_firmware(fw, name, device)
#endif

#if LINUX_VERSION_IS_LESS(4,17,0)
#define firmware_request_cache LINUX_I915_BACKPORT(firmware_request_cache)
static inline int firmware_request_cache(struct device *device, const char *name)
{
	return 0;
}
#endif

#endif /* __BACKPORT_LINUX_FIRMWARE_H */
