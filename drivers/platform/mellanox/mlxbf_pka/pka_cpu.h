/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Mellanox Techologies, Ltd.
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

#ifndef __PKA_CPU_H__
#define __PKA_CPU_H__

#include <linux/types.h>

#define MEGA 1000000

/*
 * Initial guess at our CPU speed.  We set this to be larger than any
 * possible real speed, so that any calculated delays will be too long,
 * rather than too short.
 *
 * Warning: use dummy value for frequency
 */
#define CPU_HZ_MAX        (1255 * MEGA) /* CPU Freq for High/Bin Chip */

/*
 * Processor speed in hertz; used in routines which might be called very
 * early in boot.
 */
static inline uint64_t pka_early_cpu_speed(void)
{
	return CPU_HZ_MAX;
}

#endif  /* __PKA_CPU_H__ */
