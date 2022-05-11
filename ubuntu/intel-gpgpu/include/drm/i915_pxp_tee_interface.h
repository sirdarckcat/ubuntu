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
/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2020 Intel Corporation
 *
 * Authors:
 * Vitaly Lubart <vitaly.lubart@intel.com>
 */

#ifndef _I915_PXP_TEE_INTERFACE_H_
#define _I915_PXP_TEE_INTERFACE_H_

#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/scatterlist.h>

/**
 * struct i915_pxp_component_ops - ops for PXP services.
 * @owner: Module providing the ops
 * @send: sends data to PXP
 * @receive: receives data from PXP
 */
struct i915_pxp_component_ops {
	/**
	 * @owner: owner of the module provding the ops
	 */
	struct module *owner;

	int (*send)(struct device *dev, const void *message, size_t size);
	int (*recv)(struct device *dev, void *buffer, size_t size);
	ssize_t (*gsc_command)(struct device *dev, u8 client_id, u32 fence_id,
			       struct scatterlist *sg_in, size_t total_in_len,
			       struct scatterlist *sg_out);

};

/**
 * struct i915_pxp_component - Used for communication between i915 and TEE
 * drivers for the PXP services
 * @tee_dev: device that provide the PXP service from TEE Bus.
 * @pxp_ops: Ops implemented by TEE driver, used by i915 driver.
 */
struct i915_pxp_component {
	struct device *tee_dev;
	const struct i915_pxp_component_ops *ops;

	/* To protect the above members. */
	struct mutex mutex;
};

#endif /* _I915_TEE_PXP_INTERFACE_H_ */
