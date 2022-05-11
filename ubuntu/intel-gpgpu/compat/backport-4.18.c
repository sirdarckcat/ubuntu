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
/*
 * Copyright (C) 2018 Intel Corporation
 */

#include <linux/hrtimer.h>

time64_t ktime_get_boottime_seconds(void)
{
	return ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
}
EXPORT_SYMBOL_GPL(ktime_get_boottime_seconds);
