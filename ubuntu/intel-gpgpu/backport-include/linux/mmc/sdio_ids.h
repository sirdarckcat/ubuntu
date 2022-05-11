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
#ifndef __BACKPORT_MMC_SDIO_IDS_H
#define __BACKPORT_MMC_SDIO_IDS_H
#include <linux/version.h>
#include_next <linux/mmc/sdio_ids.h>

#ifndef SDIO_CLASS_BT_AMP
#define SDIO_CLASS_BT_AMP	0x09	/* Type-A Bluetooth AMP interface */
#endif

#ifndef SDIO_DEVICE_ID_MARVELL_8688WLAN
#define SDIO_DEVICE_ID_MARVELL_8688WLAN		0x9104
#endif

#endif /* __BACKPORT_MMC_SDIO_IDS_H */
