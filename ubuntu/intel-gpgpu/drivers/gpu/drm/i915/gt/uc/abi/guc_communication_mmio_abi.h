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
 * Copyright © 2014-2021 Intel Corporation
 */

#ifndef _ABI_GUC_COMMUNICATION_MMIO_ABI_H
#define _ABI_GUC_COMMUNICATION_MMIO_ABI_H

/**
 * DOC: GuC MMIO based communication
 *
 * The MMIO based communication between Host and GuC relies on special
 * hardware registers which format could be defined by the software
 * (so called scratch registers).
 *
 * Each MMIO based message, both Host to GuC (H2G) and GuC to Host (G2H)
 * messages, which maximum length depends on number of available scratch
 * registers, is directly written into those scratch registers.
 *
 * For Gen9+, there are 16 software scratch registers 0xC180-0xC1B8,
 * but no H2G command takes more than 4 parameters and the GuC firmware
 * itself uses an 4-element array to store the H2G message.
 *
 * For Gen11+, there are additional 4 registers 0x190240-0x19024C, which
 * are, regardless on lower count, preferred over legacy ones.
 *
 * The MMIO based communication is mainly used during driver initialization
 * phase to setup the `CTB based communication`_ that will be used afterwards.
 */

#define GUC_MAX_MMIO_MSG_LEN		4

/**
 * DOC: MMIO HXG Message
 *
 * Format of the MMIO messages follows definitions of `HXG Message`_.
 *
 *  +---+-------+--------------------------------------------------------------+
 *  |   | Bits  | Description                                                  |
 *  +===+=======+==============================================================+
 *  | 0 |  31:0 |                                                              |
 *  +---+-------+                                                              |
 *  |...|       | [Embedded `HXG Message`_]                                    |
 *  +---+-------+                                                              |
 *  | n |  31:0 |                                                              |
 *  +---+-------+--------------------------------------------------------------+
 */

#endif /* _ABI_GUC_COMMUNICATION_MMIO_ABI_H */
