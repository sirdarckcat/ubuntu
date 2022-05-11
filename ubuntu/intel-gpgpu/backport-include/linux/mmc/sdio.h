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
#ifndef __BACKPORT_MMC_SDIO_H
#define __BACKPORT_MMC_SDIO_H
#include <linux/version.h>
#include_next <linux/mmc/sdio.h>

/* backports b4625dab */
#ifndef SDIO_CCCR_REV_3_00
#define  SDIO_CCCR_REV_3_00    3       /* CCCR/FBR Version 3.00 */
#endif
#ifndef SDIO_SDIO_REV_3_00
#define  SDIO_SDIO_REV_3_00    4       /* SDIO Spec Version 3.00 */
#endif

#ifndef SDIO_BUS_ECSI
#define  SDIO_BUS_ECSI		0x20	/* Enable continuous SPI interrupt */
#endif
#ifndef SDIO_BUS_SCSI
#define  SDIO_BUS_SCSI		0x40	/* Support continuous SPI interrupt */
#endif

#endif /* __BACKPORT_MMC_SDIO_H */
