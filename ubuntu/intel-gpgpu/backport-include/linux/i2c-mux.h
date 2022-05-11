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
#ifndef __BACKPORT_LINUX_I2C_MUX_H
#define __BACKPORT_LINUX_I2C_MUX_H
#include_next <linux/i2c-mux.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,5,0)
#define i2c_add_mux_adapter(parent, mux_dev, mux_priv, force_nr, chan_id, class, select, deselect) \
	i2c_add_mux_adapter(parent, mux_priv, force_nr, chan_id, select, deselect)
#elif LINUX_VERSION_IS_LESS(3,7,0)
#define i2c_add_mux_adapter(parent, mux_dev, mux_priv, force_nr, chan_id, class, select, deselect) \
	i2c_add_mux_adapter(parent, mux_dev, mux_priv, force_nr, chan_id, select, deselect)
#endif

#endif /* __BACKPORT_LINUX_I2C_MUX_H */
