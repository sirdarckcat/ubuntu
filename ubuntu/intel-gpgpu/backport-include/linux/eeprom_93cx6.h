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
#ifndef _COMPAT_LINUX_EEPROM_93CX6_H
#define _COMPAT_LINUX_EEPROM_93CX6_H 1

#include_next <linux/eeprom_93cx6.h>

#ifndef PCI_EEPROM_WIDTH_93C86
#define PCI_EEPROM_WIDTH_93C86	8
#endif /* PCI_EEPROM_WIDTH_93C86 */

#endif	/* _COMPAT_LINUX_EEPROM_93CX6_H */
