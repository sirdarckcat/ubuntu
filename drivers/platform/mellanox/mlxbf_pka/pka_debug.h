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

#ifndef __PKA_DEBUG_H__
#define __PKA_DEBUG_H__

/* PKA library bitmask. Use those bits to enable debug messages. */
#define PKA_DRIVER              0x0001
#define PKA_DEV                 0x0002
#define PKA_RING                0x0004
#define PKA_QUEUE               0x0008
#define PKA_MEM                 0x0010
#define PKA_USER                0x0020
#define PKA_TESTS               0x0040
/* PKA debug mask. This indicates the debug/verbosity level. */
#define PKA_DEBUG_LIB_MASK      0x0040

#define PKA_PRINT(lib, fmt, args...) \
	({ pr_info(#lib": "fmt, ##args); })

#define PKA_ERROR(lib, fmt, args...) \
	({ pr_err(#lib": %s: error: "fmt, __func__, ##args); })

#define PKA_DEBUG(lib, fmt, args...) \
	({								\
		if (lib & PKA_DEBUG_LIB_MASK)				\
			pr_debug(#lib": %s: "fmt, __func__, ##args);	\
	})

#define PKA_PANIC(lib, msg, args...) \
	({								\
		pr_info(#lib": %s: panic: "msg, __func__, ##args);	\
		panic(msg, ##args);                                     \
	})

#endif /* __PKA_DEBUG_H__ */
