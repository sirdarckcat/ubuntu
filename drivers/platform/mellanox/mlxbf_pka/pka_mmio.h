/* SPDX-License-Identifier: GPL-2.0
 *
 *  Copyright (C) 2018 Mellanox Techologies, Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License v2.0 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __PKA_MMIO_H__
#define __PKA_MMIO_H__

#include <linux/io.h>

/* Macros for standard MMIO functions. */
#define pka_mmio_read(addr)         readq_relaxed(addr)
#define pka_mmio_write(addr, val)   writeq_relaxed((val), (addr))

#endif /* __PKA_MMIO_H__ */
