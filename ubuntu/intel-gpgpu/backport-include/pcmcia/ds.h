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
#ifndef __BACKPORT_PCMCIA_DS_H
#define __BACKPORT_PCMCIA_DS_H
#include_next <pcmcia/ds.h>

#ifndef module_pcmcia_driver
/**
 * backport of:
 *
 * commit 6ed7ffddcf61f668114edb676417e5fb33773b59
 * Author: H Hartley Sweeten <hsweeten@visionengravers.com>
 * Date:   Wed Mar 6 11:24:44 2013 -0700
 *
 *     pcmcia/ds.h: introduce helper for pcmcia_driver module boilerplate
 */

/**
 * module_pcmcia_driver() - Helper macro for registering a pcmcia driver
 * @__pcmcia_driver: pcmcia_driver struct
 *
 * Helper macro for pcmcia drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only use
 * this macro once, and calling it replaces module_init() and module_exit().
 */
#define module_pcmcia_driver(__pcmcia_driver) \
	module_driver(__pcmcia_driver, pcmcia_register_driver, \
			pcmcia_unregister_driver)
#endif

#endif /* __BACKPORT_PCMCIA_DS_H */
