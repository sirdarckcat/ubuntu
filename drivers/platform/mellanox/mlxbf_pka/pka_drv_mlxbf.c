// SPDX-License-Identifier: GPL-2.0

/*
 *  Mellanox Public Key Accelerator (PKA) driver
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vfio.h>
#include <linux/iommu.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/hw_random.h>

#include "pka_dev.h"

#define PKA_DRIVER_VERSION      "v1.0"
#define PKA_DRIVER_NAME         "pka-vfio"

#define PKA_DRIVER_DESCRIPTION  "BlueField PKA VFIO driver"

#define PKA_DEVICE_COMPAT       "mlx,mlxbf-pka"
#define PKA_VFIO_DEVICE_COMPAT	"mlx,mlxbf-pka-vfio"

#define PKA_DEVICE_ACPIHID      "MLNXBF10"
#define PKA_VFIO_DEVICE_ACPIHID "MLNXBF11"

#define PKA_VFIO_OFFSET_SHIFT   40
#define PKA_VFIO_OFFSET_MASK    (((u64)(1) << PKA_VFIO_OFFSET_SHIFT) - 1)

#define PKA_VFIO_OFFSET_TO_INDEX(off)   \
	(off >> PKA_VFIO_OFFSET_SHIFT)

#define PKA_VFIO_INDEX_TO_OFFSET(index) \
	((u64)(index) << PKA_VFIO_OFFSET_SHIFT)

static DEFINE_MUTEX(pka_drv_lock);

static uint32_t pka_device_cnt;
static uint32_t pka_vfio_device_cnt;

const char pka_compat[]      = PKA_DEVICE_COMPAT;
const char pka_vfio_compat[] = PKA_VFIO_DEVICE_COMPAT;

const char pka_acpihid[]      = PKA_DEVICE_ACPIHID;
const char pka_vfio_acpihid[] = PKA_VFIO_DEVICE_ACPIHID;

struct pka_info {
	struct device *dev;	/* the device this info belongs to */
	const char    *name;	/* device name */
	const char    *version;	/* device driver version */
	const char    *compat;
	const char    *acpihid;
	uint8_t        flag;
	struct module *module;
	void          *priv;	/* optional private data */
};

/* defines for pka_info->flags */
#define PKA_DRIVER_FLAG_VFIO_DEVICE        1
#define PKA_DRIVER_FLAG_DEVICE             2

enum {
	PKA_REVISION_1 = 1,
	PKA_REVISION_2,
};

struct pka_platdata {
	struct platform_device *pdev;
	struct pka_info  *info;
	spinlock_t        lock;
	unsigned long     irq_flags;
};

/* Bits in pka_platdata.irq_flags */
enum {
	PKA_IRQ_DISABLED = 0,
};

struct pka_vfio_region {
	u64             off;
	u64             addr;
	resource_size_t size;
	u32             flags;
	u32            type;
	void __iomem  *ioaddr;
};

/* defines for pka_vfio_region->type */
#define PKA_VFIO_RES_TYPE_NONE      0
#define PKA_VFIO_RES_TYPE_WORDS     1	/* info control/status words */
#define PKA_VFIO_RES_TYPE_CNTRS     2	/* count registers */
#define PKA_VFIO_RES_TYPE_MEM       4	/* window RAM region */

#define PKA_DRIVER_VFIO_DEV_MAX     PKA_MAX_NUM_RINGS

struct pka_vfio_device {
	struct pka_info *info;
	struct device   *device;
	int32_t          group_id;
	uint32_t         device_id;
	uint32_t         parent_device_id;
	struct mutex     mutex;
	uint32_t         flags;
	struct module   *parent_module;
	struct pka_dev_ring_t  *ring;
	uint32_t         num_regions;
	struct pka_vfio_region *regions;
};

#define PKA_DRIVER_DEV_MAX                PKA_MAX_NUM_IO_BLOCKS
#define PKA_DRIVER_VFIO_NUM_REGIONS_MAX   PKA_MAX_NUM_RING_RESOURCES

/* defines for region index */
#define PKA_VFIO_REGION_WORDS_IDX         0
#define PKA_VFIO_REGION_CNTRS_IDX         1
#define PKA_VFIO_REGION_MEM_IDX           2

struct pka_device {
	struct pka_info *info;
	struct device   *device;
	uint32_t         device_id;
	uint8_t          fw_id;         /* firmware identifier */
	struct mutex     mutex;
	struct resource *resource;
	struct pka_dev_shim_t *shim;
	long             irq;		/* interrupt number */
	struct hwrng     rng;
};

/* defines for pka_device->irq */
#define PKA_IRQ_CUSTOM          -1
#define PKA_IRQ_NONE             0

/* Hardware interrupt handler */
static irqreturn_t pka_drv_irq_handler(int irq, void *device)
{
	struct pka_device      *pka_dev = (struct pka_device *)device;
	struct platform_device *pdev = to_platform_device(pka_dev->device);
	struct pka_platdata    *priv = platform_get_drvdata(pdev);

	PKA_DEBUG(PKA_DRIVER,
		  "handle irq in device %u\n", pka_dev->device_id);

	/* Just disable the interrupt in the interrupt controller */

	spin_lock(&priv->lock);
	if (!__test_and_set_bit(PKA_IRQ_DISABLED, &priv->irq_flags))
		disable_irq_nosync(irq);
	spin_unlock(&priv->lock);

	return IRQ_HANDLED;
}

static int pka_drv_register_irq(struct pka_device *pka_dev)
{
	if (pka_dev->irq && (pka_dev->irq != PKA_IRQ_CUSTOM)) {
		/*
		 * Allow sharing the irq among several devices (child devices
		 * so far)
		 */
		return request_irq(pka_dev->irq,
				   (irq_handler_t) pka_drv_irq_handler,
				   IRQF_SHARED, pka_dev->info->name,
				   pka_dev);
	}

	return -ENXIO;
}

static int pka_drv_vfio_regions_init(struct pka_vfio_device *vfio_dev)
{
	struct pka_vfio_region *region;
	struct pka_dev_ring_t  *ring;
	struct pka_dev_res_t   *res;
	uint64_t shim_base;
	uint32_t num_regions;

	ring = vfio_dev->ring;
	if (!ring || !ring->shim)
		return -ENXIO;

	num_regions           = ring->resources_num;
	vfio_dev->num_regions = num_regions;
	vfio_dev->regions     =
	    kcalloc(num_regions, sizeof(struct pka_vfio_region), GFP_KERNEL);
	if (!vfio_dev->regions)
		return -ENOMEM;

	shim_base = ring->shim->base;

	/* Information words region */
	res    = &ring->resources.info_words;
	region = &vfio_dev->regions[PKA_VFIO_REGION_WORDS_IDX];
	/* map offset to the physical address */
	region->off    = PKA_VFIO_INDEX_TO_OFFSET(PKA_VFIO_REGION_WORDS_IDX);
	region->addr   = res->base + shim_base;
	region->size   = res->size;
	region->type   = PKA_VFIO_RES_TYPE_WORDS;
	region->flags |= (VFIO_REGION_INFO_FLAG_MMAP |
			  VFIO_REGION_INFO_FLAG_READ |
			  VFIO_REGION_INFO_FLAG_WRITE);

	/* Count regiters region */
	res    = &ring->resources.counters;
	region = &vfio_dev->regions[PKA_VFIO_REGION_CNTRS_IDX];
	/* map offset to the physical address */
	region->off    = PKA_VFIO_INDEX_TO_OFFSET(PKA_VFIO_REGION_CNTRS_IDX);
	region->addr   = res->base + shim_base;
	region->size   = res->size;
	region->type   = PKA_VFIO_RES_TYPE_CNTRS;
	region->flags |= (VFIO_REGION_INFO_FLAG_MMAP |
			  VFIO_REGION_INFO_FLAG_READ |
			  VFIO_REGION_INFO_FLAG_WRITE);

	/* Window ram region */
	res    = &ring->resources.window_ram;
	region = &vfio_dev->regions[PKA_VFIO_REGION_MEM_IDX];
	/* map offset to the physical address */
	region->off    = PKA_VFIO_INDEX_TO_OFFSET(PKA_VFIO_REGION_MEM_IDX);
	region->addr   = res->base + shim_base;
	region->size   = res->size;
	region->type   = PKA_VFIO_RES_TYPE_MEM;
	region->flags |= (VFIO_REGION_INFO_FLAG_MMAP |
			  VFIO_REGION_INFO_FLAG_READ |
			  VFIO_REGION_INFO_FLAG_WRITE);

	return 0;
}

static void pka_drv_vfio_regions_cleanup(struct pka_vfio_device *vfio_dev)
{
	/* clear vfio device regions */
	vfio_dev->num_regions = 0;
	kfree(vfio_dev->regions);
}

static int pka_drv_vfio_open(void *device_data)
{
	struct pka_vfio_device *vfio_dev = device_data;
	struct pka_info        *info     = vfio_dev->info;

	int error;

	PKA_DEBUG(PKA_DRIVER,
		"open vfio device %u (device_data:%p)\n",
		vfio_dev->device_id, vfio_dev);

	if (!try_module_get(info->module))
		return -ENODEV;

	/* Initialize regions */
	error = pka_drv_vfio_regions_init(vfio_dev);
	if (error) {
		PKA_ERROR(PKA_DRIVER, "failed to initialize regions\n");
		module_put(info->module);
		return error;
	}

	error = pka_dev_open_ring(vfio_dev->device_id);
	if (error) {
		PKA_ERROR(PKA_DRIVER,
			"failed to open ring %u\n", vfio_dev->device_id);
		pka_drv_vfio_regions_cleanup(vfio_dev);
		module_put(info->module);
		return error;
	}

	return 0;
}

static void pka_drv_vfio_release(void *device_data)
{
	struct pka_vfio_device *vfio_dev = device_data;
	struct pka_info        *info = vfio_dev->info;

	int error;

	PKA_DEBUG(PKA_DRIVER,
			"release vfio device %u (device_data:%p)\n",
			vfio_dev->device_id, vfio_dev);

	error = pka_dev_close_ring(vfio_dev->device_id);
	if (error)
		PKA_ERROR(PKA_DRIVER,
				"failed to close ring %u\n",
				vfio_dev->device_id);

	pka_drv_vfio_regions_cleanup(vfio_dev);
	module_put(info->module);
}

static int pka_drv_vfio_mmap_region(struct pka_vfio_region region,
				    struct vm_area_struct *vma)
{
	u64 req_len, pgoff, req_start;

	req_len = vma->vm_end - vma->vm_start;
	pgoff   = vma->vm_pgoff &
			((1U << (PKA_VFIO_OFFSET_SHIFT - PAGE_SHIFT)) - 1);
	req_start = pgoff << PAGE_SHIFT;

	region.size = roundup(region.size, PAGE_SIZE);

	if (req_start + req_len > region.size)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff     = (region.addr >> PAGE_SHIFT) + pgoff;

	return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
					req_len, vma->vm_page_prot);
}

static int pka_drv_vfio_mmap(void *device_data, struct vm_area_struct *vma)
{
	struct pka_vfio_device *vfio_dev = device_data;
	struct pka_vfio_region *region;
	unsigned int            index;

	PKA_DEBUG(PKA_DRIVER, "mmap device %u\n", vfio_dev->device_id);

	index = vma->vm_pgoff >> (PKA_VFIO_OFFSET_SHIFT - PAGE_SHIFT);

	if (vma->vm_end < vma->vm_start)
		return -EINVAL;
	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;
	if (index >= vfio_dev->num_regions)
		return -EINVAL;
	if (vma->vm_start & ~PAGE_MASK)
		return -EINVAL;
	if (vma->vm_end & ~PAGE_MASK)
		return -EINVAL;

	region = &vfio_dev->regions[index];

	if (!(region->flags & VFIO_REGION_INFO_FLAG_MMAP))
		return -EINVAL;

	if (!(region->flags & VFIO_REGION_INFO_FLAG_READ)
	    && (vma->vm_flags & VM_READ))
		return -EINVAL;

	if (!(region->flags & VFIO_REGION_INFO_FLAG_WRITE)
	    && (vma->vm_flags & VM_WRITE))
		return -EINVAL;

	vma->vm_private_data = vfio_dev;

	if (region->type & PKA_VFIO_RES_TYPE_CNTRS ||
	    region->type & PKA_VFIO_RES_TYPE_MEM)
		return pka_drv_vfio_mmap_region(vfio_dev->regions[index], vma);

	if (region->type & PKA_VFIO_RES_TYPE_WORDS)
		/*
		 * Currently user space is not allowed to access this
		 * region.
		 */
		return -EINVAL;

	return -EINVAL;
}

static long pka_vfio_ioctl(void *device_data,
			   unsigned int cmd, unsigned long arg)
{
	struct pka_vfio_device *vfio_dev = device_data;

	int error = -ENOTTY;

	if (cmd == PKA_VFIO_GET_REGION_INFO) {
		struct pka_dev_region_info_t info;

		info.mem_index  = PKA_VFIO_REGION_MEM_IDX;
		info.mem_offset = vfio_dev->regions[info.mem_index].off;
		info.mem_size   = vfio_dev->regions[info.mem_index].size;

		info.reg_index  = PKA_VFIO_REGION_CNTRS_IDX;
		info.reg_offset = vfio_dev->regions[info.reg_index].off;
		info.reg_size   = vfio_dev->regions[info.reg_index].size;

		return copy_to_user((void __user *)arg, &info, sizeof(info)) ?
		    -EFAULT : 0;

	} else if (cmd == PKA_VFIO_GET_RING_INFO) {
		struct pka_dev_hw_ring_info_t *this_ring_info;
		struct pka_dev_hw_ring_info_t  hw_ring_info;

		this_ring_info = vfio_dev->ring->ring_info;

		hw_ring_info.cmmd_base      = this_ring_info->cmmd_base;
		hw_ring_info.rslt_base      = this_ring_info->rslt_base;
		hw_ring_info.size           = this_ring_info->size;
		hw_ring_info.host_desc_size = this_ring_info->host_desc_size;
		hw_ring_info.in_order       = this_ring_info->in_order;
		hw_ring_info.cmmd_rd_ptr    = this_ring_info->cmmd_rd_ptr;
		hw_ring_info.rslt_wr_ptr    = this_ring_info->rslt_wr_ptr;
		hw_ring_info.cmmd_rd_stats  = this_ring_info->cmmd_rd_ptr;
		hw_ring_info.rslt_wr_stats  = this_ring_info->rslt_wr_stats;

		return copy_to_user((void __user *)arg, &hw_ring_info,
				    sizeof(hw_ring_info)) ? -EFAULT : 0;
	}

	return error;
}

static const struct vfio_device_ops pka_vfio_ops = {
	.name    = "pka-vfio",
	.open    = pka_drv_vfio_open,
	.release = pka_drv_vfio_release,
	.ioctl   = pka_vfio_ioctl,
	.mmap    = pka_drv_vfio_mmap,
};

/*
 * Note that this function must be serialized because it calls
 * 'pka_dev_register_shim' which manipulates common counters for
 * pka devices.
 */
static int pka_drv_register_device(struct pka_device *pka_dev)
{
	uint32_t pka_shim_id;
	uint64_t pka_shim_base;
	uint64_t pka_shim_size;
	uint8_t  pka_shim_fw_id;

	/* Register Shim */
	pka_shim_id    = pka_dev->device_id;
	pka_shim_base  = pka_dev->resource->start;
	pka_shim_size  = pka_dev->resource->end - pka_shim_base;
	pka_shim_fw_id = pka_dev->fw_id;

	pka_dev->shim = pka_dev_register_shim(pka_shim_id, pka_shim_base,
					      pka_shim_size, pka_shim_fw_id);
	if (!pka_dev->shim) {
		PKA_DEBUG(PKA_DRIVER,
		  "failed to register shim id=%u, base=0x%llx, size=0x%llx\n",
		  pka_shim_id, pka_shim_base, pka_shim_size);
		return -EFAULT;
	}

	return 0;
}

static int pka_drv_unregister_device(struct pka_device *pka_dev)
{
	if (!pka_dev)
		return -EINVAL;

	if (pka_dev->shim) {
		PKA_DEBUG(PKA_DRIVER,
				"unregister device shim %u\n",
				pka_dev->shim->shim_id);
		return pka_dev_unregister_shim(pka_dev->shim);
	}

	return 0;
}

/*
 * Note that this function must be serialized because it calls
 * 'pka_dev_register_ring' which manipulates common counters for
 * vfio devices.
 */
static int pka_drv_register_vfio_device(struct pka_vfio_device *pka_vfio_dev)
{
	uint32_t ring_id;
	uint32_t shim_id;

	ring_id = pka_vfio_dev->device_id;
	shim_id = pka_vfio_dev->parent_device_id;

	pka_vfio_dev->ring = pka_dev_register_ring(ring_id, shim_id);
	if (!pka_vfio_dev->ring) {
		PKA_DEBUG(PKA_DRIVER,
				"failed to register ring %d\n", ring_id);
		return -EFAULT;
	}

	return 0;
}

static int pka_drv_unregister_vfio_device(struct pka_vfio_device *pka_vfio_dev)
{
	if (!pka_vfio_dev)
		return -EINVAL;

	if (pka_vfio_dev->ring) {
		PKA_DEBUG(PKA_DRIVER, "unregister vfio device ring %u\n",
			  pka_vfio_dev->ring->ring_id);
		return pka_dev_unregister_ring(pka_vfio_dev->ring);
	}

	return 0;
}

static const struct of_device_id pka_vfio_match[] = {
	{ .compatible = PKA_VFIO_DEVICE_COMPAT },
	{},
};

static int pka_drv_rng_read(struct hwrng *rng, void *data, size_t max,
			    bool wait)
{
	int ret;

	struct pka_device *pka_dev = container_of(rng, struct pka_device, rng);
	uint32_t          *buffer = data;

	ret = pka_dev_trng_read(pka_dev->shim, buffer, max);
	if (ret) {
		PKA_DEBUG(PKA_DRIVER,
			  "%s: failed to read random bytes ret=%d",
			  rng->name, ret);
		return 0;
	}

	return max;
}

static int pka_drv_probe_device(struct pka_info *info)
{
	struct pka_device  *pka_dev;
	struct device      *dev = info->dev;
	struct device_node *of_node = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);
	struct hwrng *trng;

	u8  revision;
	int ret;

	if (!info)
		return -EINVAL;

	pka_dev = kzalloc(sizeof(*pka_dev), GFP_KERNEL);
	if (!pka_dev)
		return -ENOMEM;

	mutex_lock(&pka_drv_lock);
	pka_device_cnt += 1;
	if (pka_device_cnt > PKA_DRIVER_DEV_MAX) {
		PKA_DEBUG(PKA_DRIVER,
			"cannot support %u devices\n", pka_device_cnt);
		kfree(pka_dev);
		mutex_unlock(&pka_drv_lock);
		return -EPERM;
	}
	pka_dev->device_id = pka_device_cnt - 1;
	mutex_unlock(&pka_drv_lock);

	pka_dev->info   = info;
	pka_dev->device = dev;
	info->flag      = PKA_DRIVER_FLAG_DEVICE;
	mutex_init(&pka_dev->mutex);

	pka_dev->resource =
	    platform_get_resource(pdev, IORESOURCE_MEM, 0);

	/* Set interrupts */
	ret = platform_get_irq(pdev, 0);
	pka_dev->irq = ret;
	if (ret == -ENXIO && of_node) {
		pka_dev->irq = PKA_IRQ_NONE;
	} else if (ret < 0) {
		PKA_ERROR(PKA_DRIVER,
			"failed to get device %u IRQ\n", pka_dev->device_id);
		return ret;
	}

	/* Register IRQ */
	ret = pka_drv_register_irq(pka_dev);
	if (ret) {
		PKA_ERROR(PKA_DRIVER,
			  "failed to register device %u IRQ\n",
			  pka_dev->device_id);
		return ret;
	}

	/*
	 * Retrieve the firmware identifier based on the device revision.
	 * Note that old platform firmware of BF1 does not support the
	 * "revision" property, thus set it by default.
	 */
	ret = device_property_read_u8(dev, "rev", &revision);
	if (ret < 0)
		revision = PKA_REVISION_1;

	switch (revision) {
	case PKA_REVISION_1:
		pka_dev->fw_id = PKA_FIRMWARE_IMAGE_0_ID;
		break;

	case PKA_REVISION_2:
		pka_dev->fw_id = PKA_FIRMWARE_IMAGE_1_ID;
		break;

	default:
		PKA_ERROR(PKA_DRIVER,
			  "device %u revision %u is not supported\n",
			  pka_dev->device_id, revision);
		return -EINVAL;
	}

	mutex_lock(&pka_drv_lock);
	ret = pka_drv_register_device(pka_dev);
	if (ret) {
		PKA_DEBUG(PKA_DRIVER, "failed to register shim id=%u\n",
				pka_dev->device_id);
		mutex_unlock(&pka_drv_lock);
		return ret;
	}
	mutex_unlock(&pka_drv_lock);

	/* Setup the TRNG, if needed */
	if (pka_dev_has_trng(pka_dev->shim)) {
		trng = &pka_dev->rng;
		trng->name = pdev->name;
		trng->read = pka_drv_rng_read;

		ret = hwrng_register(&pka_dev->rng);
		if (ret) {
			PKA_ERROR(PKA_DRIVER,
				  "failed to register trng\n");
			return ret;
		}
	}

	info->priv = pka_dev;

#ifdef BUG_SW_1127083_FIXED
	/*
	 * Create platform devices (pka-vfio) from current node.
	 * This code is reserverd for DT.
	 */
	if (of_node) {
		ret = of_platform_populate(of_node, pka_vfio_match,
					   NULL, dev);
		if (ret) {
			PKA_ERROR(PKA_DRIVER,
				"failed to create platform devices\n");
			return ret;
		}
	}
#endif

	return 0;
}

static int pka_drv_remove_device(struct platform_device *pdev)
{
	struct pka_platdata *priv = platform_get_drvdata(pdev);
	struct pka_info     *info = priv->info;
	struct pka_device   *pka_dev = (struct pka_device *)info->priv;

	if (!pka_dev) {
		PKA_ERROR(PKA_DRIVER, "failed to unregister device\n");
		return -EINVAL;
	}

	if (pka_dev_has_trng(pka_dev->shim))
		hwrng_unregister(&pka_dev->rng);

	if (pka_drv_unregister_device(pka_dev))
		PKA_ERROR(PKA_DRIVER, "failed to unregister device\n");

	return 0;
}

static int pka_drv_probe_vfio_device(struct pka_info *info)
{
	struct iommu_group     *group;
	struct pka_vfio_device *pka_vfio_dev;
	struct device          *dev = info->dev;

	int ret;

	if (!info)
		return -EINVAL;

	pka_vfio_dev = kzalloc(sizeof(*pka_vfio_dev), GFP_KERNEL);
	if (!pka_vfio_dev)
		return -ENOMEM;

	mutex_lock(&pka_drv_lock);
	pka_vfio_device_cnt += 1;
	if (pka_vfio_device_cnt > PKA_DRIVER_VFIO_DEV_MAX) {
		PKA_DEBUG(PKA_DRIVER, "cannot support %u vfio devices\n",
			  pka_vfio_device_cnt);
		kfree(pka_vfio_dev);
		mutex_unlock(&pka_drv_lock);
		return -EPERM;
	}
	pka_vfio_dev->device_id        = pka_vfio_device_cnt - 1;
	pka_vfio_dev->parent_device_id = pka_device_cnt - 1;
	mutex_unlock(&pka_drv_lock);

	pka_vfio_dev->info   = info;
	pka_vfio_dev->device = dev;
	info->flag           = PKA_DRIVER_FLAG_VFIO_DEVICE;
	mutex_init(&pka_vfio_dev->mutex);

	pka_vfio_dev->parent_module = THIS_MODULE;
	pka_vfio_dev->flags         = VFIO_DEVICE_FLAGS_PLATFORM;

	group = vfio_iommu_group_get(dev);
	if (!group) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to get IOMMU group for device %s\n",
			  info->name);
		return -EINVAL;
	}

	/*
	 * Note that this call aims to add the given child device to a vfio
	 * group. This function creates a new driver data for the device
	 * different from the structure passed as a 3rd argument - i.e.
	 * pka_vfio_dev. The struct newly created corresponds to 'vfio_device'
	 * structure which includes a field called 'device_data' that holds
	 * the initialized 'pka_vfio_dev'. So to retrieve our private data,
	 * we must call 'dev_get_drvdata()' which returns the 'vfio_device'
	 * struct and access its 'device_data' field. Here one can use
	 * 'pka_platdata' structure instead to be consistent with the parent
	 * devices, and have a common driver data structure which will be used
	 * to manage devices - 'pka_drv_remove()' for instance. Since the VFIO
	 * framework alters the driver data and introduce an indirection, it
	 * is no more relevant to have a common driver data structure. Hence,
	 * we prefer to set the struct 'pka_vfio_dev' instead to avoid
	 * indirection when we have to retrieve this structure during the
	 * open(), mmap(), and ioctl() calls. Since, this structure is used
	 * as driver data here, it will be immediately reachable for these
	 * functions (see first argument passed (void *device_data) passed
	 * to those functions).
	 */
	ret = vfio_add_group_dev(dev, &pka_vfio_ops, pka_vfio_dev);
	if (ret) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to add group device %s\n", info->name);
		goto group_put;
	}

	pka_vfio_dev->group_id = iommu_group_id(group);

	mutex_lock(&pka_drv_lock);
	/* Register VFIO device */
	ret = pka_drv_register_vfio_device(pka_vfio_dev);
	if (ret) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to register vfio device %u\n",
			  pka_vfio_dev->device_id);
		mutex_unlock(&pka_drv_lock);
		goto group_put;
	}
	mutex_unlock(&pka_drv_lock);

	info->priv = pka_vfio_dev;

	PKA_DEBUG(PKA_DRIVER,
		  "registered vfio device %u bus:%p iommu_ops:%p group:%p\n",
		  pka_vfio_dev->device_id, dev->bus, dev->bus->iommu_ops,
		  group);

	return 0;

 group_put:
	vfio_iommu_group_put(group, dev);
	return ret;
}

static int pka_drv_remove_vfio_device(struct platform_device *pdev)
{
	struct pka_vfio_device *pka_vfio_dev;
	struct device          *dev = &pdev->dev;

	pka_vfio_dev = vfio_del_group_dev(dev);
	if (pka_vfio_dev) {
		vfio_iommu_group_put(dev->iommu_group, dev);

		if (pka_drv_unregister_vfio_device(pka_vfio_dev))
			PKA_ERROR(PKA_DRIVER,
				  "failed to unregister vfio device %u\n",
				  pka_vfio_dev->device_id);
	}

	return 0;
}

static int pka_drv_of_probe(struct platform_device *pdev,
				struct pka_info *info)
{
#ifdef BUG_SW_1127083_FIXED
	struct device *dev = &pdev->dev;

	int error;

	error = device_property_read_string(dev, "compatible", &info->compat);
	if (error) {
		PKA_DEBUG(PKA_DRIVER, "cannot retrieve compat for %s\n",
			  pdev->name);
		return -EINVAL;
	}

	if (!strcmp(info->compat, pka_vfio_compat)) {
		PKA_PRINT(PKA_DRIVER, "probe vfio device %s\n",
			  pdev->name);
		error = pka_drv_probe_vfio_device(info);
		if (error) {
			PKA_DEBUG(PKA_DRIVER,
				  "failed to register vfio device compat=%s\n",
				  info->compat);
			return error;
		}

	} else if (!strcmp(info->compat, pka_compat)) {
		PKA_PRINT(PKA_DRIVER, "probe device %s\n", pdev->name);
		error = pka_drv_probe_device(info);
		if (error) {
			PKA_DEBUG(PKA_DRIVER,
				  "failed to register device compat=%s\n",
				  info->compat);
			return error;
		}
	}

	return 0;
#endif
	return -EPERM;
}

static int pka_drv_acpi_probe(struct platform_device *pdev,
			      struct pka_info *info)
{
	struct acpi_device *adev;
	struct device *dev = &pdev->dev;

	int error;

	if (acpi_disabled)
		return -ENOENT;

	adev = ACPI_COMPANION(dev);
	if (!adev) {
		PKA_DEBUG(PKA_DRIVER,
			  "ACPI companion device not found for %s\n",
			  pdev->name);
		return -ENODEV;
	}

	info->acpihid = acpi_device_hid(adev);
	if (WARN_ON(!info->acpihid))
		return -EINVAL;

	if (!strcmp(info->acpihid, pka_vfio_acpihid)) {
		error = pka_drv_probe_vfio_device(info);
		if (error) {
			PKA_DEBUG(PKA_DRIVER,
				  "failed to register vfio device %s\n",
				  pdev->name);
			return error;
		}
		PKA_DEBUG(PKA_DRIVER, "vfio device %s probed\n",
			  pdev->name);

	} else if (!strcmp(info->acpihid, pka_acpihid)) {
		error = pka_drv_probe_device(info);
		if (error) {
			PKA_DEBUG(PKA_DRIVER,
				  "failed to register device %s\n",
				  pdev->name);
			return error;
		}
		PKA_PRINT(PKA_DRIVER, "device %s probed\n", pdev->name);
	}

	return 0;
}

static int pka_drv_probe(struct platform_device *pdev)
{
	struct pka_platdata *priv;
	struct pka_info     *info;
	struct device       *dev = &pdev->dev;

	int ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	spin_lock_init(&priv->lock);
	priv->pdev = pdev;
	/* interrupt is disabled to begin with */
	priv->irq_flags = 0;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->name    = pdev->name;
	info->version = PKA_DRIVER_VERSION;
	info->module  = THIS_MODULE;
	info->dev     = dev;

	priv->info    = info;

	platform_set_drvdata(pdev, priv);

	/*
	 * There can be two kernel build combinations. One build where
	 * ACPI is not selected and another one with the ACPI.
	 *
	 * In the first case, 'pka_drv_acpi_probe' will return since
	 * acpi_disabled is 1. DT user will not see any kind of messages
	 * from ACPI.
	 *
	 * In the second case, both DT and ACPI is compiled in but the
	 * system is booting with any of these combinations.
	 *
	 * If the firmware is DT type, then acpi_disabled is 1. The ACPI
	 * probe routine terminates immediately without any messages.
	 *
	 * If the firmware is ACPI type, then acpi_disabled is 0. All other
	 * checks are valid checks. We cannot claim that this system is DT.
	 */
	ret = pka_drv_acpi_probe(pdev, info);
	if (ret)
		ret = pka_drv_of_probe(pdev, info);

	if (ret) {
		PKA_DEBUG(PKA_DRIVER, "unknown device\n");
		return ret;
	}

	return 0;
}

static int pka_drv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	/*
	 * Little hack here:
	 * The issue here is that the driver data structure which holds our
	 * initialized private data cannot be used when the 'pdev' arguments
	 * points to child device -i.e. vfio device. Indeed, during the probe
	 * function we set an initialized structure called 'priv' as driver
	 * data for all platform devices including parents devices and child
	 * devices. This driver data is unique to each device - see call to
	 * 'platform_set_drvdata()'. However, when we add the child device to
	 * a vfio group through 'vfio_add_group_dev()' call, this function
	 * creates a new driver data for the device - i.e.  a 'vfio_device'
	 * structure which includes a field called 'device_data' to hold the
	 * aforementionned initialized private data. So, to retrieve our
	 * private data, we must call 'dev_get_drvdata()' which returns the
	 * 'vfio_device' struct and access its 'device_data' field. However,
	 * this cannot be done before determining if the 'pdev' is associated
	 * with a child device or a parent device.
	 * In order to deal with that we propose this little hack which uses
	 * the iommu_group to distinguich between parent and child devices.
	 * For now, let's say it is a customized solution that works for our
	 * case. Indeed, in the current design, the private data holds some
	 * infos that defines the type of the device. The intuitive way to do
	 * that is as following:
	 *
	 * struct pka_platdata *priv = platform_get_drvdata(pdev);
	 * struct pka_info     *info = priv->info;
	 *
	 * if (info->flag == PKA_DRIVER_FLAG_VFIO_DEVICE)
	 *      return pka_drv_remove_vfio_device(info);
	 * if (info->flag == PKA_DRIVER_FLAG_DEVICE)
	 *      return pka_drv_remove_device(info);
	 *
	 * Since the returned private data of child devices -i.e vfio devices
	 * corresponds to 'vfio_device' structure, we cannot use it to
	 * differentiate between parent and child devices. This alternative
	 * solution is used instead.
	 */
	if (dev->iommu_group) {
		PKA_PRINT(PKA_DRIVER, "remove vfio device %s\n",
			  pdev->name);
		return pka_drv_remove_vfio_device(pdev);
	}

	PKA_PRINT(PKA_DRIVER, "remove device %s\n", pdev->name);
	return pka_drv_remove_device(pdev);
}

static const struct of_device_id pka_drv_match[] = {
	{ .compatible = PKA_DEVICE_COMPAT },
	{ .compatible = PKA_VFIO_DEVICE_COMPAT },
	{}
};

MODULE_DEVICE_TABLE(of, pka_drv_match);

static const struct acpi_device_id pka_drv_acpi_ids[] = {
	{ PKA_DEVICE_ACPIHID,      0 },
	{ PKA_VFIO_DEVICE_ACPIHID, 0 },
	{},
};

MODULE_DEVICE_TABLE(acpi, pka_drv_acpi_ids);

static struct platform_driver pka_drv = {
	.driver  = {
		   .name = PKA_DRIVER_NAME,
		   .of_match_table   = of_match_ptr(pka_drv_match),
		   .acpi_match_table = ACPI_PTR(pka_drv_acpi_ids),
		   },
	.probe  = pka_drv_probe,
	.remove = pka_drv_remove,
};

/* Initialize the module - Register the pka platform driver */
static int __init pka_drv_register(void)
{
	PKA_DEBUG(PKA_DRIVER, "register platform driver\n");
	return platform_driver_register(&pka_drv);
}

module_init(pka_drv_register);

/* Cleanup the module - unregister the pka platform driver */
static void __exit pka_drv_unregister(void)
{
	PKA_DEBUG(PKA_DRIVER, "unregister platform driver\n");
	platform_driver_unregister(&pka_drv);
}

module_exit(pka_drv_unregister);

MODULE_DESCRIPTION(PKA_DRIVER_DESCRIPTION);
MODULE_VERSION(PKA_DRIVER_VERSION);
MODULE_LICENSE("GPL");
