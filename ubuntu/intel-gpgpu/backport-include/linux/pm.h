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
#ifndef __BACKPORT_PM_H
#define __BACKPORT_PM_H
#include_next <linux/pm.h>

#ifndef PM_EVENT_AUTO
#define PM_EVENT_AUTO		0x0400
#endif

#ifndef PM_EVENT_SLEEP
#define PM_EVENT_SLEEP  (PM_EVENT_SUSPEND)
#endif

#ifndef PMSG_IS_AUTO
#define PMSG_IS_AUTO(msg)	(((msg).event & PM_EVENT_AUTO) != 0)
#endif

#endif /* __BACKPORT_PM_H */
