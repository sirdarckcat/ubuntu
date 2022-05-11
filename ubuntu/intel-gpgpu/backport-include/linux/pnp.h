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
#ifndef __BACKPORT_LINUX_PNP_H
#define __BACKPORT_LINUX_PNP_H
#include_next <linux/pnp.h>

#ifndef module_pnp_driver
/**
 * module_pnp_driver() - Helper macro for registering a PnP driver
 * @__pnp_driver: pnp_driver struct
 *
 * Helper macro for PnP drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_pnp_driver(__pnp_driver) \
	module_driver(__pnp_driver, pnp_register_driver, \
				    pnp_unregister_driver)
#endif

#endif /* __BACKPORT_LINUX_PNP_H */
