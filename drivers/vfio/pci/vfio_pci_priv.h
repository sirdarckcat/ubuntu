/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef VFIO_PCI_PRIV_H
#define VFIO_PCI_PRIV_H

int vfio_pci_try_reset_function(struct vfio_pci_core_device *vdev);

#ifdef CONFIG_DMA_SHARED_BUFFER
int vfio_pci_core_feature_dma_buf(struct vfio_pci_core_device *vdev,
				  struct vfio_device_p2p_dma_buf *arg);
void vfio_pci_dma_buf_cleanup(struct vfio_pci_core_device *vdev);
void vfio_pci_dma_buf_move(struct vfio_pci_core_device *vdev, bool revoked);
#else
static int
vfio_pci_core_feature_dma_buf(struct vfio_pci_core_device *vdev,
			      struct vfio_device_p2p_dma_buf *arg)
{
	return -ENOTTY;
}
static inline void vfio_pci_dma_buf_cleanup(struct vfio_pci_core_device *vdev)
{
}
static inline void vfio_pci_dma_buf_move(struct vfio_pci_core_device *vdev,
					 bool revoked)
{
}
#endif

#endif
