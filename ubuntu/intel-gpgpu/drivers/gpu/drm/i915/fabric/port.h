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
 * Copyright(c) 2019 - 2021 Intel Corporation.
 */
#ifndef PORT_H_
#define PORT_H_

#include "iaf_drv.h"

#define LINK_CRC_MODE_16B 0
#define LINK_CRC_MODE_14B 1
#define LINK_CRC_MODE_48B 2
#define LINK_CRC_MODE_PLN 3

#define LINK_FEC_MODE_NONE    0
#define LINK_FEC_MODE_F132    1
#define LINK_FEC_MODE_F528    2
#define LINK_FEC_MODE_UNKNOWN 3

void initialize_fports(struct fsubdev *sd);
void destroy_fports(struct fsubdev *sd);

int enable_fports(struct fsubdev *sd, unsigned long *lpnmask, u8 max_ports);
int disable_fports(struct fsubdev *sd, unsigned long *lpnmask, u8 max_ports);
int enable_usage_fports(struct fsubdev *sd, unsigned long *lpnmask, u8 max_ports);
int disable_usage_fports(struct fsubdev *sd, unsigned long *lpnmask, u8 max_ports);
int get_fport_status(struct fsubdev *sd, u8 lpn, struct fport_status *status);
void port_state_change_trap_handler(struct fsubdev *sd);
void port_link_width_degrade_trap_handler(struct fsubdev *sd);
void port_link_quality_indicator_trap_handler(struct fsubdev *sd);
void port_qsfp_presence_trap_handler(struct fsubdev *sd);
void port_qsfp_fault_trap_handler(struct fsubdev *sd);

#endif /* end of header file */
