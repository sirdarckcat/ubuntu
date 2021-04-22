// SPDX-License-Identifier: GPL-2.0 or BSD-3-Clause

#include <linux/acpi.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/hw_random.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vfio.h>

#include "mlxbf_pka_dev.h"


#define PKA_DRIVER_VERSION      "v3.0"
#define PKA_DRIVER_NAME         "pka-mlxbf"

#define PKA_DRIVER_DESCRIPTION  "BlueField PKA driver"

#define PKA_DEVICE_COMPAT       "mlx,mlxbf-pka"
#define PKA_RING_DEVICE_COMPAT	"mlx,mlxbf-pka-ring"

#define PKA_DEVICE_ACPIHID_BF1      "MLNXBF10"
#define PKA_RING_DEVICE_ACPIHID_BF1 "MLNXBF11"

#define PKA_DEVICE_ACPIHID_BF2      "MLNXBF20"
#define PKA_RING_DEVICE_ACPIHID_BF2 "MLNXBF21"

#define PKA_DEVICE_ACCESS_MODE  0666

#define PKA_DEVICE_RES_CNT      7
enum pka_mem_res_idx {
	PKA_ACPI_EIP154_IDX = 0,
	PKA_ACPI_WNDW_RAM_IDX,
	PKA_ACPI_ALT_WNDW_RAM_0_IDX,
	PKA_ACPI_ALT_WNDW_RAM_1_IDX,
	PKA_ACPI_ALT_WNDW_RAM_2_IDX,
	PKA_ACPI_ALT_WNDW_RAM_3_IDX,
	PKA_ACPI_CSR_IDX
};

enum pka_plat_type {
	PKA_PLAT_TYPE_BF1 = 0, /* Platform type Bluefield-1 */
	PKA_PLAT_TYPE_BF2      /* Platform type Bluefield-2 */
};

static DEFINE_MUTEX(pka_drv_lock);

static uint32_t pka_device_cnt;
static uint32_t pka_ring_device_cnt;

const char pka_compat[]      = PKA_DEVICE_COMPAT;
const char pka_ring_compat[] = PKA_RING_DEVICE_COMPAT;

const char pka_acpihid_bf1[]      = PKA_DEVICE_ACPIHID_BF1;
const char pka_ring_acpihid_bf1[] = PKA_RING_DEVICE_ACPIHID_BF1;

const char pka_acpihid_bf2[]      = PKA_DEVICE_ACPIHID_BF2;
const char pka_ring_acpihid_bf2[] = PKA_RING_DEVICE_ACPIHID_BF2;

struct pka_drv_plat_info {
	enum pka_plat_type type;
	uint8_t fw_id;
};

static struct pka_drv_plat_info pka_drv_plat[] = {
	[PKA_PLAT_TYPE_BF1] = {
		.type = PKA_PLAT_TYPE_BF1,
		.fw_id = PKA_FIRMWARE_IMAGE_0_ID
	},
	[PKA_PLAT_TYPE_BF2] = {
		.type = PKA_PLAT_TYPE_BF2,
		.fw_id = PKA_FIRMWARE_IMAGE_2_ID
	}
};

static const struct acpi_device_id pka_drv_acpi_ids[] = {
	{ PKA_DEVICE_ACPIHID_BF1, (kernel_ulong_t)&pka_drv_plat[PKA_PLAT_TYPE_BF1] },
	{ PKA_RING_DEVICE_ACPIHID_BF1, 0 },
	{ PKA_DEVICE_ACPIHID_BF2, (kernel_ulong_t)&pka_drv_plat[PKA_PLAT_TYPE_BF2] },
	{ PKA_RING_DEVICE_ACPIHID_BF2, 0 },
	{},
};

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
#define PKA_DRIVER_FLAG_RING_DEVICE        1
#define PKA_DRIVER_FLAG_DEVICE             2

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

struct pka_ring_region {
	u64             off;
	u64             addr;
	resource_size_t size;
	u32             flags;
	u32             type;
	void __iomem   *ioaddr;
};

/* defines for pka_ring_region->flags */
#define PKA_RING_REGION_FLAG_READ      (1 << 0) /* Region supports read */
#define PKA_RING_REGION_FLAG_WRITE     (1 << 1) /* Region supports write */
#define PKA_RING_REGION_FLAG_MMAP      (1 << 2) /* Region supports mmap */

/* defines for pka_ring_region->type */
#define PKA_RING_RES_TYPE_NONE      0
#define PKA_RING_RES_TYPE_WORDS     1	/* info control/status words */
#define PKA_RING_RES_TYPE_CNTRS     2	/* count registers */
#define PKA_RING_RES_TYPE_MEM       4	/* window RAM region */

#define PKA_DRIVER_RING_DEV_MAX     PKA_MAX_NUM_RINGS

struct pka_ring_device {
	struct pka_info    *info;
	struct device      *device;
	struct iommu_group *group;
	int32_t             group_id;
	uint32_t            device_id;
	uint32_t            parent_device_id;
	struct mutex        mutex;
	uint32_t            flags;
	struct module      *parent_module;
	pka_dev_ring_t     *ring;
	int                 minor;
	uint32_t            num_regions;
	struct pka_ring_region *regions;
};

#define PKA_DRIVER_DEV_MAX                PKA_MAX_NUM_IO_BLOCKS
#define PKA_DRIVER_RING_NUM_REGIONS_MAX   PKA_MAX_NUM_RING_RESOURCES

/* defines for region index */
#define PKA_RING_REGION_WORDS_IDX         0
#define PKA_RING_REGION_CNTRS_IDX         1
#define PKA_RING_REGION_MEM_IDX           2

#define PKA_RING_REGION_OFFSET_SHIFT   40
#define PKA_RING_REGION_OFFSET_MASK    \
	(((u64)(1) << PKA_RING_REGION_OFFSET_SHIFT) - 1)

#define PKA_RING_OFFSET_TO_INDEX(off)   \
	(off >> PKA_RING_REGION_OFFSET_SHIFT)

#define PKA_RING_REGION_INDEX_TO_OFFSET(index) \
	((u64)(index) << PKA_RING_REGION_OFFSET_SHIFT)

struct pka_device {
	struct pka_info *info;
	struct device   *device;
	uint32_t         device_id;
	uint8_t          fw_id;         /* firmware identifier */
	struct mutex     mutex;
	struct resource *resource[PKA_DEVICE_RES_CNT];
	pka_dev_shim_t  *shim;
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

static int pka_drv_ring_regions_init(struct pka_ring_device *ring_dev)
{
	struct pka_ring_region *region;
	pka_dev_ring_t *ring;
	pka_dev_res_t  *res;
	uint32_t        num_regions;

	ring = ring_dev->ring;
	if (!ring || !ring->shim)
		return -ENXIO;

	num_regions           = ring->resources_num;
	ring_dev->num_regions = num_regions;
	ring_dev->regions     = kcalloc(num_regions,
					sizeof(struct pka_ring_region),
					GFP_KERNEL);
	if (!ring_dev->regions)
		return -ENOMEM;

	/* Information words region */
	res    = &ring->resources.info_words;
	region = &ring_dev->regions[PKA_RING_REGION_WORDS_IDX];
	/* map offset to the physical address */
	region->off    =
		PKA_RING_REGION_INDEX_TO_OFFSET(PKA_RING_REGION_WORDS_IDX);
	region->addr   = res->base;
	region->size   = res->size;
	region->type   = PKA_RING_RES_TYPE_WORDS;
	region->flags |= (PKA_RING_REGION_FLAG_MMAP |
			  PKA_RING_REGION_FLAG_READ |
			  PKA_RING_REGION_FLAG_WRITE);

	/* Count regiters region */
	res    = &ring->resources.counters;
	region = &ring_dev->regions[PKA_RING_REGION_CNTRS_IDX];
	/* map offset to the physical address */
	region->off    =
		PKA_RING_REGION_INDEX_TO_OFFSET(PKA_RING_REGION_CNTRS_IDX);
	region->addr   = res->base;
	region->size   = res->size;
	region->type   = PKA_RING_RES_TYPE_CNTRS;
	region->flags |= (PKA_RING_REGION_FLAG_MMAP |
			  PKA_RING_REGION_FLAG_READ |
			  PKA_RING_REGION_FLAG_WRITE);

	/* Window ram region */
	res    = &ring->resources.window_ram;
	region = &ring_dev->regions[PKA_RING_REGION_MEM_IDX];
	/* map offset to the physical address */
	region->off    =
		PKA_RING_REGION_INDEX_TO_OFFSET(PKA_RING_REGION_MEM_IDX);
	region->addr   = res->base;
	region->size   = res->size;
	region->type   = PKA_RING_RES_TYPE_MEM;
	region->flags |= (PKA_RING_REGION_FLAG_MMAP |
			  PKA_RING_REGION_FLAG_READ |
			  PKA_RING_REGION_FLAG_WRITE);

	return 0;
}

static void pka_drv_ring_regions_cleanup(struct pka_ring_device *ring_dev)
{
	/* clear vfio device regions */
	ring_dev->num_regions = 0;
	kfree(ring_dev->regions);
}

static int pka_drv_ring_open(void *device_data)
{
	struct pka_ring_device *ring_dev = device_data;
	struct pka_info        *info     = ring_dev->info;
	pka_ring_info_t         ring_info;

	int error;

	PKA_DEBUG(PKA_DRIVER,
		  "open ring device %u (device_data:%p)\n",
		  ring_dev->device_id, ring_dev);

	if (!try_module_get(info->module))
		return -ENODEV;

	ring_info.ring_id = ring_dev->device_id;
	error = pka_dev_open_ring(&ring_info);
	if (error) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to open ring %u\n", ring_dev->device_id);
		module_put(info->module);
		return error;
	}

	/* Initialize regions */
	error = pka_drv_ring_regions_init(ring_dev);
	if (error) {
		PKA_DEBUG(PKA_DRIVER, "failed to initialize regions\n");
		pka_dev_close_ring(&ring_info);
		module_put(info->module);
		return error;
	}

	return 0;
}

static void pka_drv_ring_release(void *device_data)
{
	struct pka_ring_device *ring_dev = device_data;
	struct pka_info        *info = ring_dev->info;
	pka_ring_info_t         ring_info;

	int error;

	PKA_DEBUG(PKA_DRIVER,
		  "release ring device %u (device_data:%p)\n",
		  ring_dev->device_id, ring_dev);

	pka_drv_ring_regions_cleanup(ring_dev);

	ring_info.ring_id = ring_dev->device_id;
	error = pka_dev_close_ring(&ring_info);
	if (error)
		PKA_DEBUG(PKA_DRIVER,
			  "failed to close ring %u\n",
			  ring_dev->device_id);

	module_put(info->module);
}

static int pka_drv_ring_mmap_region(struct pka_ring_region region,
				    struct vm_area_struct *vma)
{
	u64 req_len, pgoff, req_start;

	req_len = vma->vm_end - vma->vm_start;
	pgoff   = vma->vm_pgoff &
		((1U << (PKA_RING_REGION_OFFSET_SHIFT - PAGE_SHIFT)) - 1);
	req_start = pgoff << PAGE_SHIFT;

	region.size = roundup(region.size, PAGE_SIZE);

	if (req_start + req_len > region.size)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff     = (region.addr >> PAGE_SHIFT) + pgoff;

	return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
					req_len, vma->vm_page_prot);
}

static int pka_drv_ring_mmap(void *device_data, struct vm_area_struct *vma)
{
	struct pka_ring_device *ring_dev = device_data;
	struct pka_ring_region *region;
	unsigned int            index;

	PKA_DEBUG(PKA_DRIVER, "mmap device %u\n", ring_dev->device_id);

	index = vma->vm_pgoff >> (PKA_RING_REGION_OFFSET_SHIFT - PAGE_SHIFT);

	if (vma->vm_end < vma->vm_start)
		return -EINVAL;
	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;
	if (index >= ring_dev->num_regions)
		return -EINVAL;
	if (vma->vm_start & ~PAGE_MASK)
		return -EINVAL;
	if (vma->vm_end & ~PAGE_MASK)
		return -EINVAL;

	region = &ring_dev->regions[index];

	if (!(region->flags & PKA_RING_REGION_FLAG_MMAP))
		return -EINVAL;

	if (!(region->flags & PKA_RING_REGION_FLAG_READ)
	    && (vma->vm_flags & VM_READ))
		return -EINVAL;

	if (!(region->flags & PKA_RING_REGION_FLAG_WRITE)
	    && (vma->vm_flags & VM_WRITE))
		return -EINVAL;

	vma->vm_private_data = ring_dev;

	if (region->type & PKA_RING_RES_TYPE_CNTRS ||
	    region->type & PKA_RING_RES_TYPE_MEM)
		return pka_drv_ring_mmap_region(ring_dev->regions[index], vma);

	if (region->type & PKA_RING_RES_TYPE_WORDS)
		/*
		 * Currently user space is not allowed to access this
		 * region.
		 */
		return -EINVAL;

	return -EINVAL;
}

static long pka_drv_ring_ioctl(void *device_data,
			   unsigned int cmd, unsigned long arg)
{
	struct pka_ring_device *ring_dev = device_data;

	int error = -ENOTTY;

	if (cmd == PKA_RING_GET_REGION_INFO) {
		pka_dev_region_info_t info;

		info.mem_index  = PKA_RING_REGION_MEM_IDX;
		info.mem_offset = ring_dev->regions[info.mem_index].off;
		info.mem_size   = ring_dev->regions[info.mem_index].size;

		info.reg_index  = PKA_RING_REGION_CNTRS_IDX;
		info.reg_offset = ring_dev->regions[info.reg_index].off;
		info.reg_size   = ring_dev->regions[info.reg_index].size;

		return copy_to_user((void __user *)arg, &info, sizeof(info)) ?
			-EFAULT : 0;

	} else if (cmd == PKA_GET_RING_INFO) {
		pka_dev_hw_ring_info_t *this_ring_info;
		pka_dev_hw_ring_info_t  hw_ring_info;

		this_ring_info = ring_dev->ring->ring_info;

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
	} else if (cmd == PKA_CLEAR_RING_COUNTERS) {
		return pka_dev_clear_ring_counters(ring_dev->ring);
	} else if (cmd == PKA_GET_RANDOM_BYTES) {
		pka_dev_trng_info_t *trng_data;
		pka_dev_shim_t *shim;
		bool trng_present;
		uint32_t byte_cnt;
		uint32_t *data;
		int ret;

		ret = -ENOENT;
		shim = ring_dev->ring->shim;
		trng_data = (pka_dev_trng_info_t *)arg;
		/*
		 * We need byte count which is multiple of 4 as
		 * required by pka_dev_trng_read() interface.
		 */
		byte_cnt = round_up(trng_data->count, 4);

		data = kzalloc(byte_cnt, GFP_KERNEL);
		if (data == NULL) {
			PKA_DEBUG(PKA_DRIVER, "failed to allocate memory.\n");
			return -ENOMEM;
		}

		trng_present = pka_dev_has_trng(shim);
		if (!trng_present) {
			kfree(data);
			return ret;
		}

		ret = pka_dev_trng_read(shim, data, byte_cnt);
		if (ret) {
			PKA_DEBUG(PKA_DRIVER, "TRNG failed %d\n", ret);
			kfree(data);
			return ret;
		}

		ret = copy_to_user((void __user *)(trng_data->data), data, trng_data->count);
		kfree(data);
		return ret ? -EFAULT : 0;
	}

	return error;
}

#ifdef CONFIG_PKA_VFIO_IOMMU
static const struct vfio_device_ops pka_ring_vfio_ops = {
	.name    = PKA_DRIVER_NAME,
	.open    = pka_drv_ring_open,
	.release = pka_drv_ring_release,
	.ioctl   = pka_drv_ring_ioctl,
	.mmap    = pka_drv_ring_mmap,
};

static int pka_drv_add_ring_device(struct pka_ring_device *ring_dev)
{
	struct device *dev = ring_dev->device;
	int ret;

	ring_dev->parent_module = THIS_MODULE;
	ring_dev->flags         = VFIO_DEVICE_FLAGS_PLATFORM;

	ring_dev->group = vfio_iommu_group_get(dev);
	if (!ring_dev->group) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to get IOMMU group for device %d\n",
			  ring_dev->device_id);
		return -EINVAL;
	}

	/*
	 * Note that this call aims to add the given child device to a vfio
	 * group. This function creates a new driver data for the device
	 * different from the structure passed as a 3rd argument - i.e.
	 * pka_ring_dev. The struct newly created corresponds to 'vfio_device'
	 * structure which includes a field called 'device_data' that holds
	 * the initialized 'pka_ring_dev'. So to retrieve our private data,
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
	ret = vfio_add_group_dev(dev, &pka_ring_vfio_ops, ring_dev);
	if (ret) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to add group device %d\n",
			  ring_dev->device_id);
		vfio_iommu_group_put(ring_dev->group, dev);
		return ret;
	}

	ring_dev->group_id = iommu_group_id(ring_dev->group);

	PKA_DEBUG(PKA_DRIVER,
		  "ring device %d bus:%p iommu_ops:%p group:%p\n",
		  ring_dev->device_id,
		  dev->bus,
		  dev->bus->iommu_ops,
		  ring_dev->group);

	return 0;
}

static struct pka_ring_device *pka_drv_del_ring_device(struct device *dev)
{
	struct pka_ring_device *ring_dev;

	ring_dev = vfio_del_group_dev(dev);
	if (ring_dev)
		vfio_iommu_group_put(dev->iommu_group, dev);

	return ring_dev;
}

static int pka_drv_init_class(void)
{
	return 0;
}

static void pka_drv_destroy_class(void)
{
}
#else
static struct pka {
	struct class *class;
	struct idr    ring_idr;
	struct mutex  ring_lock;
	struct cdev   ring_cdev;
	dev_t         ring_devt;
} pka;

static int pka_drv_open(struct inode *inode, struct file *filep)
{
	struct pka_ring_device *ring_dev;
	int ret;

	ring_dev = idr_find(&pka.ring_idr, iminor(inode));
	if (!ring_dev) {
		PKA_ERROR(PKA_DRIVER,
			  "failed to find idr for device %d\n",
			  ring_dev->device_id);
		return -ENODEV;
	}

	ret = pka_drv_ring_open(ring_dev);
	if (ret)
		return ret;

	filep->private_data = ring_dev;
	return 0;
}

static int pka_drv_release(struct inode *inode, struct file *filep)
{
	struct pka_ring_device *ring_dev = filep->private_data;

	filep->private_data = NULL;
	pka_drv_ring_release(ring_dev);

	return 0;
}

static int pka_drv_mmap(struct file *filep, struct vm_area_struct *vma)
{
	return pka_drv_ring_mmap(filep->private_data, vma);
}

static long pka_drv_unlocked_ioctl(struct file *filep,
				   unsigned int cmd, unsigned long arg)
{
	return pka_drv_ring_ioctl(filep->private_data, cmd, arg);
}

static const struct file_operations pka_ring_fops = {
	.owner          = THIS_MODULE,
	.open           = pka_drv_open,
	.release        = pka_drv_release,
	.unlocked_ioctl = pka_drv_unlocked_ioctl,
	.mmap           = pka_drv_mmap,
};

static int pka_drv_add_ring_device(struct pka_ring_device *ring_dev)
{
	struct device *dev = ring_dev->device;

	ring_dev->minor = idr_alloc(&pka.ring_idr,
				    ring_dev, 0, MINORMASK + 1, GFP_KERNEL);
	if (ring_dev->minor < 0) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to alloc minor to device %d\n",
			  ring_dev->device_id);
		return ring_dev->minor;
	}

	dev = device_create(pka.class, NULL,
			    MKDEV(MAJOR(pka.ring_devt), ring_dev->minor),
			    ring_dev, "%d", ring_dev->device_id);
	if (IS_ERR(dev)) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to create device %d\n",
			  ring_dev->device_id);
		idr_remove(&pka.ring_idr, ring_dev->minor);
		return PTR_ERR(dev);
	}

	PKA_DEBUG(PKA_DRIVER,
		  "ring device %d minor:%d\n",
		  ring_dev->device_id, ring_dev->minor);

	return 0;
}

static struct pka_ring_device *pka_drv_del_ring_device(struct device *dev)
{
	struct platform_device *pdev     =
		container_of(dev, struct platform_device, dev);
	struct pka_platdata        *priv = platform_get_drvdata(pdev);
	struct pka_info            *info = priv->info;
	struct pka_ring_device *ring_dev = info->priv;

	if (ring_dev) {
		device_destroy(pka.class, MKDEV(MAJOR(pka.ring_devt),
						ring_dev->minor));
		idr_remove(&pka.ring_idr, ring_dev->minor);
	}

	return ring_dev;
}

static char *pka_drv_devnode(struct device *dev, umode_t *mode)
{
	if (mode != NULL)
		*mode = PKA_DEVICE_ACCESS_MODE;
	return kasprintf(GFP_KERNEL, "pka/%s", dev_name(dev));
}

static int pka_drv_init_class(void)
{
	int ret;

	idr_init(&pka.ring_idr);
	/* /sys/class/pka/$RING */
	pka.class = class_create(THIS_MODULE, "pka");
	if (IS_ERR(pka.class))
		return PTR_ERR(pka.class);

	/* /dev/pka/$RING */
	pka.class->devnode = pka_drv_devnode;

	ret = alloc_chrdev_region(&pka.ring_devt, 0, MINORMASK, "pka");
	if (ret)
		goto err_alloc_chrdev;

	cdev_init(&pka.ring_cdev, &pka_ring_fops);
	ret = cdev_add(&pka.ring_cdev, pka.ring_devt, MINORMASK);
	if (ret)
		goto err_cdev_add;

	return 0;

err_cdev_add:
	unregister_chrdev_region(pka.ring_devt, MINORMASK);
err_alloc_chrdev:
	class_destroy(pka.class);
	pka.class = NULL;
	return ret;
}

static void pka_drv_destroy_class(void)
{
	idr_destroy(&pka.ring_idr);
	cdev_del(&pka.ring_cdev);
	unregister_chrdev_region(pka.ring_devt, MINORMASK);
	class_destroy(pka.class);
	pka.class = NULL;
}
#endif

static void pka_drv_get_mem_res(struct pka_device *pka_dev,
				struct pka_dev_mem_res *mem_res,
				uint64_t wndw_ram_off_mask)
{
	enum pka_mem_res_idx acpi_mem_idx;

	acpi_mem_idx = PKA_ACPI_EIP154_IDX;
	mem_res->wndw_ram_off_mask = wndw_ram_off_mask;

	/* PKA EIP154 MMIO base address*/
	mem_res->eip154_base = pka_dev->resource[acpi_mem_idx]->start;
	mem_res->eip154_size = pka_dev->resource[acpi_mem_idx]->end -
			       mem_res->eip154_base + 1;
	acpi_mem_idx++;

	/* PKA window ram base address*/
	mem_res->wndw_ram_base = pka_dev->resource[acpi_mem_idx]->start;
	mem_res->wndw_ram_size = pka_dev->resource[acpi_mem_idx]->end -
				 mem_res->wndw_ram_base + 1;
	acpi_mem_idx++;

	/* PKA alternate window ram base address
	 * Note: Here the size of all the alt window ram is same, depicted by
	 * 'alt_wndw_ram_size' variable. All alt window ram resources are read
	 * here even though not all of them are used currently.
	 */
	mem_res->alt_wndw_ram_0_base = pka_dev->resource[acpi_mem_idx]->start;
	mem_res->alt_wndw_ram_size   = pka_dev->resource[acpi_mem_idx]->end -
				       mem_res->alt_wndw_ram_0_base + 1;

	if (mem_res->alt_wndw_ram_size != PKA_WINDOW_RAM_REGION_SIZE)
		PKA_ERROR(PKA_DRIVER,
		"Alternate Window RAM size read from ACPI is incorrect.\n");

	acpi_mem_idx++;

	mem_res->alt_wndw_ram_1_base = pka_dev->resource[acpi_mem_idx]->start;
	acpi_mem_idx++;

	mem_res->alt_wndw_ram_2_base = pka_dev->resource[acpi_mem_idx]->start;
	acpi_mem_idx++;

	mem_res->alt_wndw_ram_3_base = pka_dev->resource[acpi_mem_idx]->start;
	acpi_mem_idx++;

	/* PKA CSR base address*/
	mem_res->csr_base = pka_dev->resource[acpi_mem_idx]->start;
	mem_res->csr_size = pka_dev->resource[acpi_mem_idx]->end -
			    mem_res->csr_base + 1;
}

/*
 * Note that this function must be serialized because it calls
 * 'pka_dev_register_shim' which manipulates common counters for
 * pka devices.
 */
static int pka_drv_register_device(struct pka_device *pka_dev,
				   uint64_t wndw_ram_off_mask)
{
	uint32_t pka_shim_id;
	uint8_t  pka_shim_fw_id;
	struct pka_dev_mem_res mem_res;

	/* Register Shim */
	pka_shim_id    = pka_dev->device_id;
	pka_shim_fw_id = pka_dev->fw_id;

	pka_drv_get_mem_res(pka_dev, &mem_res, wndw_ram_off_mask);

	pka_dev->shim = pka_dev_register_shim(pka_shim_id, pka_shim_fw_id,
					      &mem_res);
	if (!pka_dev->shim) {
		PKA_DEBUG(PKA_DRIVER,
		  "failed to register shim id=%u\n", pka_shim_id);
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
static int pka_drv_register_ring_device(struct pka_ring_device *ring_dev)
{
	uint32_t ring_id;
	uint32_t shim_id;

	ring_id = ring_dev->device_id;
	shim_id = ring_dev->parent_device_id;

	ring_dev->ring = pka_dev_register_ring(ring_id, shim_id);
	if (!ring_dev->ring) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to register ring device %u\n", ring_id);
		return -EFAULT;
	}

	return 0;
}

static int pka_drv_unregister_ring_device(struct pka_ring_device *ring_dev)
{
	uint32_t ring_id;

	if (!ring_dev)
		return -EINVAL;

	ring_id = ring_dev->ring->ring_id;

	if (ring_dev->ring) {
		PKA_DEBUG(PKA_DRIVER, "unregister ring device %u\n", ring_id);
		return pka_dev_unregister_ring(ring_dev->ring);
	}

	return 0;
}

static const struct of_device_id pka_ring_match[] = {
	{ .compatible = PKA_RING_DEVICE_COMPAT },
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
	const struct acpi_device_id *aid;
	struct pka_drv_plat_info *plat_info;
	uint64_t wndw_ram_off_mask;
	int ret;
	enum pka_mem_res_idx acpi_mem_idx;

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

	for (acpi_mem_idx = PKA_ACPI_EIP154_IDX;
		acpi_mem_idx < PKA_DEVICE_RES_CNT; acpi_mem_idx++) {
		pka_dev->resource[acpi_mem_idx] =
			platform_get_resource(pdev, IORESOURCE_MEM, acpi_mem_idx);
	}

	/* Window ram offset mask is platform dependent */
	aid = acpi_match_device(pka_drv_acpi_ids, dev);
	if (!aid)
		return -ENODEV;

	plat_info = (struct pka_drv_plat_info *)aid->driver_data;
	if (plat_info->type <= PKA_PLAT_TYPE_BF2) {
		wndw_ram_off_mask = PKA_WINDOW_RAM_OFFSET_MASK1;
	} else {
		PKA_ERROR(PKA_DRIVER, "Invalid platform type: %d\n",
				(int)plat_info->type);
		return -EINVAL;
	}

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

	/* Firmware version */
	pka_dev->fw_id = plat_info->fw_id;

	mutex_lock(&pka_drv_lock);
	ret = pka_drv_register_device(pka_dev, wndw_ram_off_mask);
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
	 * Create platform devices (pka-ring) from current node.
	 * This code is reserverd for DT.
	 */
	if (of_node) {
		ret = of_platform_populate(of_node, pka_ring_match,
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

static int pka_drv_probe_ring_device(struct pka_info *info)
{
	struct pka_ring_device *ring_dev;
	struct device          *dev = info->dev;

	int ret;

	if (!info)
		return -EINVAL;

	ring_dev = kzalloc(sizeof(*ring_dev), GFP_KERNEL);
	if (!ring_dev)
		return -ENOMEM;

	mutex_lock(&pka_drv_lock);
	pka_ring_device_cnt += 1;
	if (pka_ring_device_cnt > PKA_DRIVER_RING_DEV_MAX) {
		PKA_DEBUG(PKA_DRIVER, "cannot support %u ring devices\n",
			  pka_ring_device_cnt);
		kfree(ring_dev);
		mutex_unlock(&pka_drv_lock);
		return -EPERM;
	}
	ring_dev->device_id        = pka_ring_device_cnt - 1;
	ring_dev->parent_device_id = pka_device_cnt - 1;
	mutex_unlock(&pka_drv_lock);

	ring_dev->info   = info;
	ring_dev->device = dev;
	info->flag       = PKA_DRIVER_FLAG_RING_DEVICE;
	mutex_init(&ring_dev->mutex);

	ret = pka_drv_add_ring_device(ring_dev);
	if (ret) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to add ring device %u\n",
			  ring_dev->device_id);
		kfree(ring_dev);
		return ret;
	}

	mutex_lock(&pka_drv_lock);
	/* Register ring device */
	ret = pka_drv_register_ring_device(ring_dev);
	if (ret) {
		PKA_DEBUG(PKA_DRIVER,
			  "failed to register ring device %u\n",
			  ring_dev->device_id);
		mutex_unlock(&pka_drv_lock);
		goto err_register_ring;
	}
	mutex_unlock(&pka_drv_lock);

	info->priv = ring_dev;

	return 0;

 err_register_ring:
	pka_drv_del_ring_device(dev);
	kfree(ring_dev);
	return ret;
}

static int pka_drv_remove_ring_device(struct platform_device *pdev)
{
	struct pka_ring_device *ring_dev;
	struct device          *dev = &pdev->dev;
	int ret;

	ring_dev = pka_drv_del_ring_device(dev);
	if (ring_dev) {
		ret = pka_drv_unregister_ring_device(ring_dev);
		if (ret) {
			PKA_ERROR(PKA_DRIVER,
				  "failed to unregister vfio device\n");
			return ret;
		}
		kfree(ring_dev);
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

	if (!strcmp(info->compat, pka_ring_compat)) {
		PKA_PRINT(PKA_DRIVER, "probe ring device %s\n",
			  pdev->name);
		error = pka_drv_probe_ring_device(info);
		if (error) {
			PKA_DEBUG(PKA_DRIVER,
				  "failed to register ring device compat=%s\n",
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

	if (!strcmp(info->acpihid, pka_ring_acpihid_bf1)
	|| !strcmp(info->acpihid, pka_ring_acpihid_bf2)) {
		error = pka_drv_probe_ring_device(info);
		if (error) {
			PKA_DEBUG(PKA_DRIVER,
				  "failed to register ring device %s\n",
				  pdev->name);
			return error;
		}
		PKA_DEBUG(PKA_DRIVER, "ring device %s probed\n",
			  pdev->name);

	} else if (!strcmp(info->acpihid, pka_acpihid_bf1)
		|| !strcmp(info->acpihid, pka_acpihid_bf2)) {
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
	 * if (info->flag == PKA_DRIVER_FLAG_RING_DEVICE)
	 *      return pka_drv_remove_ring_device(info);
	 * if (info->flag == PKA_DRIVER_FLAG_DEVICE)
	 *      return pka_drv_remove_ring_device(info);
	 *
	 * Since the returned private data of child devices -i.e vfio devices
	 * corresponds to 'vfio_device' structure, we cannot use it to
	 * differentiate between parent and child devices. This alternative
	 * solution is used instead.
	 */
	if (dev->iommu_group) {
		PKA_PRINT(PKA_DRIVER, "remove ring device %s\n",
			  pdev->name);
		return pka_drv_remove_ring_device(pdev);
	}

	PKA_PRINT(PKA_DRIVER, "remove device %s\n", pdev->name);
	return pka_drv_remove_device(pdev);
}

static const struct of_device_id pka_drv_match[] = {
	{ .compatible = PKA_DEVICE_COMPAT },
	{ .compatible = PKA_RING_DEVICE_COMPAT },
	{}
};

MODULE_DEVICE_TABLE(of, pka_drv_match);

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
	int ret;

	ret = pka_drv_init_class();
	if (ret) {
		PKA_ERROR(PKA_DRIVER, "failed to create class\n");
		return ret;
	}

	ret = platform_driver_register(&pka_drv);
	if (ret) {
		PKA_ERROR(PKA_DRIVER, "failed to register platform driver\n");
		return ret;
	}

	PKA_PRINT(PKA_DRIVER, "version: " PKA_DRIVER_VERSION "\n");

	return 0;
}

/* Cleanup the module - unregister the pka platform driver */
static void __exit pka_drv_unregister(void)
{
	platform_driver_unregister(&pka_drv);
	pka_drv_destroy_class();
}

module_init(pka_drv_register);
module_exit(pka_drv_unregister);

MODULE_DESCRIPTION(PKA_DRIVER_DESCRIPTION);
MODULE_VERSION(PKA_DRIVER_VERSION);
MODULE_LICENSE("Dual BSD/GPL");
