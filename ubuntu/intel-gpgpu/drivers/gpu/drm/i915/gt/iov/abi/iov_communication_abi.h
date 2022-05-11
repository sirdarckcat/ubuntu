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
 * Copyright © 2022 Intel Corporation
 */

#ifndef _ABI_IOV_COMMUNICATION_ABI_H_
#define _ABI_IOV_COMMUNICATION_ABI_H_

#include "gt/uc/abi/guc_communication_ctb_abi.h"

/**
 * DOC: IOV Communication
 *
 * The communication between VFs and PF is based on the relay messages with GuC
 * acting a proxy agent. All relay messages are defined as `CTB HXG Message`_.
 * The `IOV Message`_ is embedded in these messages as opaque payload.
 *
 * To send `IOV Message`_ to the PF, VFs are using `VF2GUC_RELAY_TO_PF`_
 * that takes the message identifier as additional parameter.
 *
 *  +--------------------------------------------------------------------------+
 *  |  `CTB Message`_                                                          |
 *  |                                                                          |
 *  +===+======================================================================+
 *  |   |  `CTB HXG Message`_                                                  |
 *  |   |                                                                      |
 *  |   +---+------------------------------------------------------------------+
 *  |   |   | `HXG Message`_                                                   |
 *  |   |   |                                                                  |
 *  |   |   +---+--------------------------------------------------------------+
 *  |   |   |   |  `HXG Request`_                                              |
 *  |   |   |   |                                                              |
 *  |   |   |   +---+----------------------------------------------------------+
 *  |   |   |   |   |  `VF2GUC_RELAY_TO_PF`_                                   |
 *  |   |   |   |   |                                                          |
 *  |   |   |   |   +------------+---------------------------------------------+
 *  |   |   |   |   |            |              +----------------------------+ |
 *  |   |   |   |   | Message ID |              |     `IOV Message`_         | |
 *  |   |   |   |   |            |              +----------------------------+ |
 *  +---+---+---+---+------------+---------------------------------------------+
 *
 * The `IOV Message`_ from a VF is delivered to the PF in `GUC2PF_RELAY_FROM_VF`_.
 * This message contains also identifier of the origin VF and message identifier
 * that is used in any replies.
 *
 *  +--------------------------------------------------------------------------+
 *  |  `CTB Message`_                                                          |
 *  |                                                                          |
 *  +===+======================================================================+
 *  |   |  `CTB HXG Message`_                                                  |
 *  |   |                                                                      |
 *  |   +---+------------------------------------------------------------------+
 *  |   |   | `HXG Message`_                                                   |
 *  |   |   |                                                                  |
 *  |   |   +---+--------------------------------------------------------------+
 *  |   |   |   |  `HXG Request`_                                              |
 *  |   |   |   |                                                              |
 *  |   |   |   +---+----------------------------------------------------------+
 *  |   |   |   |   |  `GUC2PF_RELAY_FROM_VF`_                                 |
 *  |   |   |   |   |                                                          |
 *  |   |   |   |   +------------+------------+--------------------------------+
 *  |   |   |   |   |            |            | +----------------------------+ |
 *  |   |   |   |   |   Origin   | Message ID | |     `IOV Message`_         | |
 *  |   |   |   |   |            |            | +----------------------------+ |
 *  +---+---+---+---+------------+------------+--------------------------------+
 *
 * To send `IOV Message`_ to the particular VF, PF is using `PF2GUC_RELAY_TO_VF`_
 * that takes target VF identifier and the message identifier.
 *
 *  +--------------------------------------------------------------------------+
 *  |  `CTB Message`_                                                          |
 *  |                                                                          |
 *  +===+======================================================================+
 *  |   |  `CTB HXG Message`_                                                  |
 *  |   |                                                                      |
 *  |   +---+------------------------------------------------------------------+
 *  |   |   | `HXG Message`_                                                   |
 *  |   |   |                                                                  |
 *  |   |   +---+--------------------------------------------------------------+
 *  |   |   |   |  `HXG Request`_                                              |
 *  |   |   |   |                                                              |
 *  |   |   |   +---+----------------------------------------------------------+
 *  |   |   |   |   |  `PF2GUC_RELAY_TO_VF`_                                   |
 *  |   |   |   |   |                                                          |
 *  |   |   |   |   +------------+------------+--------------------------------+
 *  |   |   |   |   |            |            | +----------------------------+ |
 *  |   |   |   |   |   Target   | Message ID | |     `IOV Message`_         | |
 *  |   |   |   |   |            |            | +----------------------------+ |
 *  +---+---+---+---+------------+------------+--------------------------------+
 *
 * The `IOV Message`_ from the PF is delivered to VFs in `GUC2VF_RELAY_FROM_PF`_.
 * The message identifier is used to match IOV requests/response messages.
 *
 *  +--------------------------------------------------------------------------+
 *  |  `CTB Message`_                                                          |
 *  |                                                                          |
 *  +===+======================================================================+
 *  |   |  `CTB HXG Message`_                                                  |
 *  |   |                                                                      |
 *  |   +---+------------------------------------------------------------------+
 *  |   |   | `HXG Message`_                                                   |
 *  |   |   |                                                                  |
 *  |   |   +---+--------------------------------------------------------------+
 *  |   |   |   |  `HXG Request`_                                              |
 *  |   |   |   |                                                              |
 *  |   |   |   +---+----------------------------------------------------------+
 *  |   |   |   |   |  `GUC2VF_RELAY_FROM_PF`_                                 |
 *  |   |   |   |   |                                                          |
 *  |   |   |   |   +------------+---------------------------------------------+
 *  |   |   |   |   |            |              +----------------------------+ |
 *  |   |   |   |   | Message ID |              |     `IOV Message`_         | |
 *  |   |   |   |   |            |              +----------------------------+ |
 *  +---+---+---+---+------------+---------------------------------------------+
 */

#endif /* _ABI_IOV_COMMUNICATION_ABI_H_ */
