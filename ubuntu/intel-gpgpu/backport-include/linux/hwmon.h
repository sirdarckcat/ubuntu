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
#ifndef __BACKPORT_LINUX_HWMON_H
#define __BACKPORT_LINUX_HWMON_H
#include_next <linux/hwmon.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,13,0)
/*
 * Backports
 *
 * commit bab2243ce1897865e31ea6d59b0478391f51812b
 * Author: Guenter Roeck <linux@roeck-us.net>
 * Date:   Sat Jul 6 13:57:23 2013 -0700
 *
 *     hwmon: Introduce hwmon_device_register_with_groups
 *
 *     hwmon_device_register_with_groups() lets callers register a hwmon device
 *     together with all sysfs attributes in a single call.
 *
 *     When using hwmon_device_register_with_groups(), hwmon attributes are attached
 *     to the hwmon device directly and no longer with its parent device.
 *
 * Signed-off-by: Guenter Roeck <linux@roeck-us.net>
 */
struct device *
hwmon_device_register_with_groups(struct device *dev, const char *name,
				  void *drvdata,
				  const struct attribute_group **groups);
struct device *
devm_hwmon_device_register_with_groups(struct device *dev, const char *name,
				       void *drvdata,
				       const struct attribute_group **groups);
#endif /* LINUX_VERSION_IS_LESS(3,13,0) */

#endif /* __BACKPORT_LINUX_HWMON_H */
