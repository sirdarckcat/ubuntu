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
#ifndef __BP_LINUX_IO_H
#define __BP_LINUX_IO_H
#include_next <linux/io.h>

#ifndef IOMEM_ERR_PTR
#define IOMEM_ERR_PTR(err) (__force void __iomem *)ERR_PTR(err)
#endif

#if LINUX_VERSION_IS_LESS(4,5,0)
#define __ioread32_copy LINUX_I915_BACKPORT(__ioread32_copy)
void __ioread32_copy(void *to, const void __iomem *from, size_t count);
#endif

#ifndef writel_relaxed
#define writel_relaxed writel_relaxed
static inline void writel_relaxed(u32 value, volatile void __iomem *addr)
{
	__raw_writel(__cpu_to_le32(value), addr);
}
#endif

#endif /* __BP_LINUX_IO_H */
