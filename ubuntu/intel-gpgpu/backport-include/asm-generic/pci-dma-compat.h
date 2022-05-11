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
#ifndef __BACKPORT_ASM_GENERIC_PCI_DMA_COMPAT_H
#define __BACKPORT_ASM_GENERIC_PCI_DMA_COMPAT_H
#include_next <asm-generic/pci-dma-compat.h>

#if LINUX_VERSION_IS_LESS(3,17,0)
#define pci_zalloc_consistent LINUX_I915_BACKPORT(pci_zalloc_consistent)
static inline void *pci_zalloc_consistent(struct pci_dev *hwdev, size_t size,
					  dma_addr_t *dma_handle)
{
	void *ret = pci_alloc_consistent(hwdev, size, dma_handle);
	if (ret)
		memset(ret, 0, size);
	return ret;
}
#endif

#endif /* __BACKPORT_ASM_GENERIC_PCI_DMA_COMPAT_H */
