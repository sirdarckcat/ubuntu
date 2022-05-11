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
#ifndef __BACKPORT_LINUX_MEI_CL_BUS_H
#define __BACKPORT_LINUX_MEI_CL_BUS_H
#include_next <linux/mei_cl_bus.h>

#if LINUX_VERSION_IS_LESS(4,3,0)
#define mei_cldev_register_event_cb(cldev, event_mask, read_cb, context) \
	mei_cl_register_event_cb(cldev, read_cb, context)
#elif LINUX_VERSION_IS_LESS(4,4,0)
#define mei_cldev_register_event_cb(cldev, event_mask, read_cb, context) \
	mei_cl_register_event_cb(cldev, event_mask, read_cb, context)
#endif

#if LINUX_VERSION_IS_LESS(4,4,0)
#define __mei_cldev_driver_register(cldrv, owner) __mei_cl_driver_register(cldrv, owner)
#define mei_cldev_driver_register(cldrv) mei_cl_driver_register(cldrv)
#define mei_cldev_driver_unregister(cldrv) mei_cl_driver_unregister(cldrv)
#define mei_cldev_send(cldev, buf, length) mei_cl_send(cldev, buf, length)
#define mei_cldev_recv(cldev, buf, length) mei_cl_recv(cldev, buf, length)
#define mei_cldev_get_drvdata(cldev) mei_cl_get_drvdata(cldev)
#define mei_cldev_set_drvdata(cldev, data) mei_cl_set_drvdata(cldev, data)
#define mei_cldev_enable(cldev) mei_cl_enable_device(cldev)
#define mei_cldev_disable(cldev) mei_cl_disable_device(cldev)
#endif

#endif /* __BACKPORT_LINUX_MEI_CL_BUS_H */
