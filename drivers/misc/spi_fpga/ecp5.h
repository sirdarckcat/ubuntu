// SPDX-License-Identifier: GPL-2.0
/*
 *  ecp5.h - QED spi lattice ecp5 command definitions.
 *
 *  Copyright (C) 2022 Lantronix Inc.
 *
 */

#ifndef ECP5_H_
#define ECP5_H_

#define ECP5_FM "lattice-ecp5.bit"

#define FPGA_CFG_SPI_DEV_CFG_LATTICE 2

#define FPGA_FW_SPI_DEV_CFG 1
#define CMD_READ_ID 0xe0 /* class A */
#define CMD_READ_STATUS 0x3c /* class A */
#define CMD_CHECK_BUSY 0xf0 /* class A */
#define CMD_ISC_ENABLE 0xc6 /* class C */
#define CMD_ISC_DISABLE 0x26 /* class C */
#define CMD_LSC_REFRESH 0x79 /* class D */
#define CMD_LSC_BITSTREAM_BURST 0x7a /* class C */
#define LFE5U_45_ID 0x41112043
#define LFE5UM_45_ID 0x01112043

#endif /* ECP5_H_ */
