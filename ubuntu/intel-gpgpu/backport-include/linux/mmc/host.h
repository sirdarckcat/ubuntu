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
#ifndef _BACKPORTLINUX_MMC_HOST_H
#define _BACKPORTLINUX_MMC_HOST_H
#include_next <linux/mmc/host.h>
#include <linux/version.h>
#include <linux/mmc/card.h>

#if LINUX_VERSION_IS_LESS(3,16,0)
#define mmc_card_hs LINUX_I915_BACKPORT(mmc_card_hs)
static inline int mmc_card_hs(struct mmc_card *card)
{
	return card->host->ios.timing == MMC_TIMING_SD_HS ||
		card->host->ios.timing == MMC_TIMING_MMC_HS;
}
#endif /* LINUX_VERSION_IS_LESS(3,16,0) */

#endif /* _BACKPORTLINUX_MMC_HOST_H */
