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
 * Copyright(c) 2020, Intel Corporation. All rights reserved.
 */

#ifndef __INTEL_PXP_TEE_H__
#define __INTEL_PXP_TEE_H__

#include "intel_pxp.h"

int intel_pxp_tee_component_init(struct intel_pxp *pxp);
void intel_pxp_tee_component_fini(struct intel_pxp *pxp);

int intel_pxp_tee_cmd_create_arb_session(struct intel_pxp *pxp,
					 int arb_session_id);

int intel_pxp_tee_stream_message(struct intel_pxp *pxp,
				 u8 client_id, u32 fence_id,
				 void *msg_in, size_t msg_in_len,
				 void *msg_out, size_t msg_out_len);

#endif /* __INTEL_PXP_TEE_H__ */
