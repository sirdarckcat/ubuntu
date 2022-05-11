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
// SPDX-License-Identifier: MIT
/*
 * Copyright © 2020 Intel Corporation
 */

#include <linux/dma-resv.h>

#include "dma_resv_utils.h"

void dma_resv_prune(struct dma_resv *resv)
{
	if (dma_resv_trylock(resv)) {
		if (dma_resv_test_signaled(resv, true))
			dma_resv_add_excl_fence(resv, NULL);
		dma_resv_unlock(resv);
	}
}
