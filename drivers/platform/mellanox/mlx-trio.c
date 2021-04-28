// SPDX-License-Identifier: GPL-2.0
/*
 * TRIO driver for Mellanox BlueField SoC
 *
 * Copyright (c) 2018, Mellanox Technologies. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/acpi.h>
#include <linux/arm-smccc.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include <uapi/linux/psci.h>

#include "trio_regs.h"

#define DRIVER_NAME		"mlx-trio"
#define DRIVER_VERSION		"0.3"
#define DRIVER_DESCRIPTION	"Mellanox TRIO PCIe host controller driver"

/* SMC return codes */
#define SMCCC_ACCESS_VIOLATION (-4)

/* SMC function identifiers */
#define MLNX_WRITE_REG_64		(0x8200000B)
#define MLNX_READ_REG_64		(0x8200000C)
#define MLNX_SIP_SVC_UID		(0x8200ff01)
#define MLNX_SIP_SVC_VERSION		(0x8200ff03)

#define MLNX_TRIO_SVC_REQ_MAJOR 0
#define MLNX_TRIO_SVC_MIN_MINOR 4

#define TRIO_NUM_IRQS 17
#define L3_PROFILE_NUM	(L3C_PROF_RD_MISS__LENGTH / L3C_PROF_RD_MISS__STRIDE)

/* The PUSH_DMA_EVT_CTR wrapped. */
#define TRIO_PUSH_DMA_EVT_CTR_INT_BIT 10

/* The MAP_EVT_CTR wrapped. */
#define TRIO_MAP_EVT_CTR_INT_BIT 11

enum trio_int_events {
	TRIO_MAC_INT = 0,
	TRIO_RSH_FULL_ERR_INT,
	TRIO_MSG_Q_FULL_ERR_INT,
	TRIO_MSG_Q_ARRIVED_INT,
	TRIO_MMIO_ERR_INT,
	TRIO_MAP_UNCLAIMED_INT,
	TRIO_RSH_SIZE_ERR_INT,
	TRIO_PIO_ECAM_ERR_INT,
	TRIO_PIO_CPL_ERR_INT,
	TRIO_MMIO_PROT_ERR_INT,
	TRIO_PUSH_DMA_EVT_CTR_INT,
	TRIO_MAP_EVT_CTR_INT,
	TRIO_PIO_DISABLED_INT,
	TRIO_REM_MMIO_ERR_INT,
	TRIO_ERR_MSG_COR_INT,
	TRIO_ERR_MSG_NONFATAL_INT,
	TRIO_ERR_MSG_FATAL_INT,
};

struct trio_event_info {
	const char *name;
	int additional_info;
};

static const struct trio_event_info trio_events[TRIO_NUM_IRQS] = {
	[TRIO_MAC_INT] = {
		.name = "MAC Interrupt",
		.additional_info = -1,
	},
	[TRIO_RSH_FULL_ERR_INT] = {
		.name = "RShim Full Error",
		.additional_info = -1,
	},
	[TRIO_MSG_Q_FULL_ERR_INT] = {
		.name = "Msg Queue Full Error",
		.additional_info = -1,
	},
	[TRIO_MSG_Q_ARRIVED_INT] = {
		.name = "Msg Arrived Interrupt",
		.additional_info = -1,
	},
	[TRIO_MMIO_ERR_INT] = {
		.name = "MMIO Error",
		.additional_info = TRIO_MMIO_ERROR_INFO,
	},
	[TRIO_MAP_UNCLAIMED_INT] = {
		.name = "Packet Unclaimed Error",
		.additional_info = TRIO_MAP_ERR_STS,
	},
	[TRIO_RSH_SIZE_ERR_INT] = {
		.name = "RShim Size Error",
		.additional_info = -1,
	},
	[TRIO_PIO_ECAM_ERR_INT] = {
		.name = "PIO ECAM Error",
		.additional_info = -1,
	},
	[TRIO_PIO_CPL_ERR_INT] = {
		.name = "PIO Completion Error",
		.additional_info = TRIO_TILE_PIO_CPL_ERR_STS,
	},
	[TRIO_MMIO_PROT_ERR_INT] = {
		.name = "MMIO Protection level Violation",
		.additional_info = -1,
	},
	[TRIO_PUSH_DMA_EVT_CTR_INT] = {
		.name = "PUSH_DMA_CTR wrapped",
		.additional_info = -1,
	},
	[TRIO_MAP_EVT_CTR_INT] = {
		.name = "MAP_EVT_CTR wrapped",
		.additional_info = -1,
	},
	[TRIO_PIO_DISABLED_INT] = {
		.name = "Access to disabled PIO region",
		.additional_info = -1,
	},
	[TRIO_REM_MMIO_ERR_INT] = {
		.name = "Remote Buffer MMIO Error",
		.additional_info = -1,
	},
	[TRIO_ERR_MSG_COR_INT] = {
		.name = "Correctable error message received",
		.additional_info = -1,
	},
	[TRIO_ERR_MSG_NONFATAL_INT] = {
		.name = "Nonfatal error message received",
		.additional_info = -1,
	},
	[TRIO_ERR_MSG_FATAL_INT] = {
		.name = "Fatal error message received",
		.additional_info = -1,
	},
};

enum l3_profile_type {
	LRU_PROFILE = 0,	/* 0 is the default behavior. */
	NVME_PROFILE,
	L3_PROFILE_TYPE_NUM,
};

static const char *l3_profiles[L3_PROFILE_TYPE_NUM] = {
	[LRU_PROFILE] = "Strict_LRU",
	[NVME_PROFILE] = "NVMeOF_suitable"
};

/*
 * The default profile each L3 profile would get.
 * The current setting would make profile 1 the NVMe suitable profile
 * and the rest of the profiles LRU profile.
 * Note that profile 0 should be configured as LRU as this is the
 * default profile.
 */
static const enum l3_profile_type default_profile[L3_PROFILE_NUM] = {
	[1] = NVME_PROFILE,
};

struct event_context {
	int event_num;
	int irq;
	struct trio_context *trio;
};

struct trio_context {
	/* The kernel structure representing the device. */
	struct platform_device	*pdev;

	/* Argument to be passed back to the IRQ handler */
	struct event_context *events;

	/*
	 * Reg base addr, will be memmapped if sreg_use_smcs is false.
	 * Otherwise, this is a physical address.
	 */
	void __iomem *mmio_base;

	int trio_index;

	/* Name of the bus this TRIO corresponds to */
	const char *bus;

	/* The PCI device this TRIO corresponds to */
	struct pci_dev *trio_pci;

	/* Number of platform_irqs for this device */
	uint32_t num_irqs;

	/* Access regs with smcs if true */
	bool sreg_use_smcs;

	/* verification table for trio */
	uint32_t sreg_trio_tbl;
};

static int secure_writeq(struct trio_context *trio, uint64_t value,
		void __iomem *addr)
{
	struct arm_smccc_res res;
	int status;

	arm_smccc_smc(MLNX_WRITE_REG_64, trio->sreg_trio_tbl, value,
			(uintptr_t) addr, 0, 0, 0, 0, &res);

	status = res.a0;

	switch (status) {
	/*
	 * Note: PSCI_RET_NOT_SUPPORTED is used here to maintain compatibility
	 * with older kernels that do not have SMCCC_RET_NOT_SUPPORTED
	 */
	case PSCI_RET_NOT_SUPPORTED:
		dev_err(&trio->pdev->dev,
			"%s: required SMC unsupported\n",
			__func__);
		return -1;
	case SMCCC_ACCESS_VIOLATION:
		dev_err(&trio->pdev->dev,
			"%s: could not access register at %px\n",
			__func__,
			addr);
		return -1;
	default:
		return 0;
	}
}

static int trio_writeq(struct trio_context *trio, uint64_t value,
		void __iomem *addr)
{
	if (trio->sreg_use_smcs)
		return secure_writeq(trio, value, addr);
	else {
		writeq(value, addr);
		return 0;
	}
}

static int secure_readq(struct trio_context *trio, void __iomem *addr,
		uint64_t *result)
{
	struct arm_smccc_res res;
	int status;

	arm_smccc_smc(MLNX_READ_REG_64, trio->sreg_trio_tbl, (uintptr_t) addr,
			0, 0, 0, 0, 0, &res);

	status = res.a0;

	switch (status) {
	/*
	 * Note: PSCI_RET_NOT_SUPPORTED is used here to maintain compatibility
	 * with older kernels that do not have SMCCC_RET_NOT_SUPPORTED
	 */
	case PSCI_RET_NOT_SUPPORTED:
		dev_err(&trio->pdev->dev,
			"%s: required SMC unsupported\n", __func__);
		return -1;
	case SMCCC_ACCESS_VIOLATION:
		dev_err(&trio->pdev->dev,
			"%s: could not read register %px\n",
			__func__,
			addr);
		return -1;
	default:
		*result = (uint64_t)res.a1;
		return 0;
	}
}

static int trio_readq(struct trio_context *trio, void __iomem *addr,
		uint64_t *result)
{
	if (trio->sreg_use_smcs)
		return secure_readq(trio, addr, result);
	else {
		*result = readq(addr);
		return 0;
	}
}

static irqreturn_t trio_irq_handler(int irq, void *arg)
{
	struct event_context *ctx = (struct event_context *)arg;
	struct trio_context *trio = ctx->trio;

	dev_err_ratelimited(&trio->pdev->dev,
		"mlx_trio: TRIO %d received IRQ %d event %d (%s)\n",
		trio->trio_index, irq, ctx->event_num,
		trio_events[ctx->event_num].name);

	if (trio_events[ctx->event_num].additional_info != -1) {
		uint64_t info;
		trio_readq(trio, trio->mmio_base +
				trio_events[ctx->event_num].additional_info,
				&info);
		dev_err_ratelimited(&trio->pdev->dev,
			"mlx_trio: Addition IRQ info: %llx\n", info);
	}

	return IRQ_HANDLED;
}

static ssize_t current_profile_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int profile_num;
	struct trio_context *trio;
	struct platform_device *pdev;

	TRIO_DEV_CTL_t tdc;

	pdev = to_platform_device(dev);
	trio = platform_get_drvdata(pdev);

	if (trio_readq(trio, trio->mmio_base + TRIO_DEV_CTL, &tdc.word)) {
		return -EIO;
	}

	if (tdc.l3_profile_ovd == 0)
		profile_num = -1;
	else
		profile_num = tdc.l3_profile_val;

	return sprintf(buf, "%d\n", profile_num);
}

static int set_l3cache_profile(struct trio_context *trio, long profile_num)
{
	TRIO_DEV_CTL_t tdc;

	if (trio_readq(trio, trio->mmio_base + TRIO_DEV_CTL, &tdc.word)) {
		return -EIO;
	}

	if (profile_num == -1) {
		dev_info(&trio->pdev->dev, "Unlink %s profile\n", trio->bus);

		tdc.l3_profile_ovd = 0;
	} else if (profile_num < L3_PROFILE_NUM && profile_num >= 0) {
		dev_info(&trio->pdev->dev, "Change %s to profile %ld\n",
				trio->bus, profile_num);

		tdc.l3_profile_ovd = 1;
		tdc.l3_profile_val = profile_num;
	} else {
		dev_err(&trio->pdev->dev, "Profile number out of range.");
		return -EINVAL;
	}

	if (trio_writeq(trio, tdc.word, trio->mmio_base + TRIO_DEV_CTL)) {
		return -EIO;
	}

	return 0;
}

static ssize_t current_profile_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int err;
	long profile_num;
	struct trio_context *trio;
	struct platform_device *pdev;

	pdev = container_of(dev, struct platform_device, dev);
	trio = platform_get_drvdata(pdev);

	err = kstrtol(buf, 10, &profile_num);
	if (err)
		return err;

	err = set_l3cache_profile(trio, profile_num);
	if (err)
		return err;

	return count;
}

static DEVICE_ATTR_RW(current_profile);

static ssize_t available_profiles_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int i;
	ssize_t line_size;
	ssize_t len = 0;

	for (i = 0; i < L3_PROFILE_NUM; i++) {
		line_size = sprintf(buf, "%d %s\n", i,
					l3_profiles[default_profile[i]]);
		buf += line_size;
		len += line_size;
	}
	return len;
}

static DEVICE_ATTR_RO(available_profiles);

static int trio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct trio_context *trio;
	int i, j, ret, irq;
	int trio_bus, trio_device, trio_function;
	struct resource *res;
	struct arm_smccc_res smc_res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_warn(dev, "%s: failed to find reg resource 0\n", __func__);
		return -ENODEV;
	}

	trio = kzalloc(sizeof(struct trio_context), GFP_KERNEL);
	if (!trio)
		return -ENOMEM;

	platform_set_drvdata(pdev, trio);
	trio->pdev = pdev;

	/* Determine whether to use SMCs or not. */
	if (device_property_read_u32(&pdev->dev, "sec_reg_block",
			&trio->sreg_trio_tbl)) {
		trio->sreg_use_smcs = false;
	} else {
		/*
		 * Ensure we have the UUID we expect for the Mellanox service.
		 */
		arm_smccc_smc(MLNX_SIP_SVC_UID, 0, 0, 0, 0, 0, 0, 0, &smc_res);
		if (smc_res.a0 != 0x89c036b4 || smc_res.a1 != 0x11e6e7d7 ||
			smc_res.a2 != 0x1a009787 || smc_res.a3 != 0xc4bf00ca) {
			dev_err(&pdev->dev,
				"Mellanox SMC service not available\n");
			return -EINVAL;
		}

		/*
		 * Check service version to see if we actually do support the
		 * needed SMCs. If we have the calls we need, mark support for
		 * them in the trio struct.
		 */
		arm_smccc_smc(MLNX_SIP_SVC_VERSION, 0, 0, 0, 0, 0, 0, 0,
				&smc_res);
		if (smc_res.a0 == MLNX_TRIO_SVC_REQ_MAJOR &&
			smc_res.a1 >= MLNX_TRIO_SVC_MIN_MINOR) {
			trio->sreg_use_smcs = true;
		} else {
			dev_err(&pdev->dev,
				"Required SMCs are not supported.\n");

			return -EINVAL;
		}
	}

	if (device_property_read_string(dev, "bus_number", &trio->bus)) {
		dev_warn(dev, "%s: failed to retrieve Trio bus name\n",
			 __func__);
		ret = -ENODEV;
		goto err;
	}

	if (device_property_read_u32(dev, "num_irqs", &trio->num_irqs))
		trio->num_irqs = TRIO_NUM_IRQS;
	trio->events = kzalloc(sizeof(struct event_context) * trio->num_irqs,
			       GFP_KERNEL);
	if (!trio->events) {
		ret = -ENOMEM;
		goto err;
	}

	/* Map registers */
	if (!trio->sreg_use_smcs) {
		trio->mmio_base = devm_ioremap_resource(&pdev->dev, res);

		if (IS_ERR(trio->mmio_base)) {
			dev_warn(dev, "%s: ioremap failed for mmio_base %llx err %p\n",
				 __func__, res->start, trio->mmio_base);
			ret = PTR_ERR(trio->mmio_base);
			goto err;
		}
	} else
		trio->mmio_base = (void __iomem *) res->start;

	for (i = 0; i < trio->num_irqs; ++i) {
		struct event_context *ctx = &trio->events[i];
		int dri_ret;

                switch (i) {
                case TRIO_PUSH_DMA_EVT_CTR_INT_BIT:
                case TRIO_MAP_EVT_CTR_INT_BIT:
			/*
			 * These events are not errors, they just indicate
			 * that a performance counter wrapped.  We may want
			 * the performance counter driver to register for them.
			 */
			continue;
                default:
			break;
                }

		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			dev_warn(dev, "%s: failed to get plat irq %d ret %d\n",
				 __func__, i, irq);
			for (j = i - 1; j >= 0; j--) {
				ctx = &trio->events[j];
				devm_free_irq(&pdev->dev, ctx->irq, ctx);
			}
			ret = -ENXIO;
			goto err;
		}
		ctx->event_num = i;
		ctx->trio = trio;
		ctx->irq = irq;
		dri_ret = devm_request_irq(&pdev->dev, irq, trio_irq_handler, 0,
					   dev_name(dev), ctx);

		dev_dbg(dev, "%s: request_irq returns %d %d->%d\n", __func__,
			 dri_ret, i, irq);
	}

	/* Create the L3 cache profile on this device */
	device_create_file(dev, &dev_attr_current_profile);
	device_create_file(dev, &dev_attr_available_profiles);

	/*
	 * Get the corresponding PCI device this trio maps to.
	 * If the bus number can't be read properly, no symlinks are created.
	 */
	if (sscanf(trio->bus, "%d:%d.%d", &trio_bus, &trio_device,
		   &trio_function) != 3) {
		dev_warn(dev, "Device [%s] not valid\n", trio->bus);
		return 0;
	}

	/* trio_device is also the index of the TRIO */
	trio->trio_index = trio_device;

	/* The PCI domain/segment would always be 0 here. */
	trio->trio_pci =
		pci_get_domain_bus_and_slot(0, trio_bus,
					    (trio_device << 3) + trio_function);

	/* Add the symlink from the TRIO to the PCI device */
	if (trio->trio_pci != NULL) {
		if (sysfs_create_link(&dev->kobj, &trio->trio_pci->dev.kobj,
				      "pcie_slot")) {
			pci_dev_put(trio->trio_pci);
			trio->trio_pci = NULL;
			dev_warn(dev, "Failed to create symblink for %s\n",
				 trio->bus);
		}
	} else
		dev_warn(dev, "Device %s not found\n", trio->bus);

	dev_info(dev, "v" DRIVER_VERSION " probed\n");
	return 0;
err:
	dev_warn(dev, "Error probing trio\n");
	if (trio->events)
		kfree(trio->events);
	kfree(trio);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int trio_remove(struct platform_device *pdev)
{
	struct trio_context *trio = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int i;

	for (i = 0; i < trio->num_irqs; ++i) {
		struct event_context *ctx = &trio->events[i];

		if (ctx->irq)
			devm_free_irq(&pdev->dev, ctx->irq, ctx);
	}
	device_remove_file(dev, &dev_attr_current_profile);
	device_remove_file(dev, &dev_attr_available_profiles);

	/* Delete the symlink and decrement the reference count. */
	if (trio->trio_pci != NULL) {
		sysfs_remove_link(&dev->kobj, "pcie_slot");
		pci_dev_put(trio->trio_pci);
	}
	platform_set_drvdata(pdev, NULL);
	kfree(trio->events);
	kfree(trio);

	return 0;
}

static const struct acpi_device_id trio_acpi_ids[] = {
	{"MLNXBF06", 0},
	{},
};

MODULE_DEVICE_TABLE(acpi, trio_acpi_ids);
static struct platform_driver mlx_trio_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.acpi_match_table = ACPI_PTR(trio_acpi_ids),
	},
	.probe = trio_probe,
	.remove = trio_remove,
};

static int __init trio_init(void)
{
	int ret;

	ret = platform_driver_register(&mlx_trio_driver);
	if (ret)
		pr_err("Failed to register trio driver.\n");

	return ret;
}

static void __exit trio_exit(void)
{
	platform_driver_unregister(&mlx_trio_driver);
}

module_init(trio_init);
module_exit(trio_exit);

MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_AUTHOR("Mellanox Technologies");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
