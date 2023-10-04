// SPDX-License-Identifier: GPL-2.0
/*
 * Mellanox BlueField HCA firmware burning driver.
 *
 *  Copyright (C) 2017 Mellanox Technologies.  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License v2.0 as published by
 *  the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * This driver supports burning firmware for the embedded HCA in the
 * BlueField SoC.  Typically firmware is burned through the PCI mlx5
 * driver directly, but when the existing firmware is not yet installed
 * or invalid, the PCI mlx5 driver has no endpoint to bind to, and we
 * use this driver instead.  It provides a character device that gives
 * access to the same hardware registers at the same offsets as the
 * mlx5 PCI configuration space does.
 *
 * The first 1 MB of the space is available through the TRIO HCA
 * mapping.  However, the efuse area (128 bytes at offset 0x1c1600) is
 * not available through the HCA mapping, but is available by mapping
 * the TYU via the RSHIM, so we make it virtually appear at the
 * correct offset in this driver.
 */

#include <linux/acpi.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define HCA_SIZE (1024 * 1024UL)
static phys_addr_t hca_pa;
static __iomem void *hca_va;

#define TYU_SIZE 0x80UL
#define TYU_OFFSET 0x1c1600
static phys_addr_t tyu_pa;
static __iomem void *tyu_va;

#define CRSPACE_SIZE (2 * 1024 * 1024)

/*
 * A valid I/O must be entirely within CR space and not extend into
 * any unmapped areas of CR space.  We don't truncate I/O that extends
 * past the end of the CR space region (unlike the behavior of, for
 * example, simple_read_from_buffer) but instead just call the whole
 * I/O invalid.  We also enforce 4-byte alignment for all I/O.
 */
static bool valid_range(loff_t offset, size_t len)
{
	if (offset % 4 != 0 || len % 4 != 0)
		return false;  /* unaligned */
	if (offset >= 0 && offset + len <= HCA_SIZE)
		return true;   /* inside the HCA space */
	if (offset >= TYU_OFFSET && offset + len <= TYU_OFFSET + TYU_SIZE)
		return true;   /* inside the TYU space */
	return false;
}

/*
 * Read and write to CR space offsets; we assume valid_range().
 * Data crossing the TRIO CR Space bridge gets byte-swapped, so we swap
 * it back.
 */

static u32 crspace_readl(int offset)
{
	if (offset < TYU_OFFSET)
		return swab32(readl_relaxed(hca_va + offset));
	else
		return readl_relaxed(tyu_va + offset - TYU_OFFSET);
}

static void crspace_writel(u32 data, int offset)
{
	if (offset < TYU_OFFSET)
		writel_relaxed(swab32(data), hca_va + offset);
	else
		writel_relaxed(data, tyu_va + offset - TYU_OFFSET);
}

/*
 * Note that you can seek to illegal areas within the livefish device,
 * but you won't be able to read or write there.
 */
static loff_t livefish_llseek(struct file *filp, loff_t offset, int whence)
{
	if (offset % 4 != 0)
		return -EINVAL;
	return fixed_size_llseek(filp, offset, whence, CRSPACE_SIZE);
}

static ssize_t livefish_read(struct file *filp, char __user *to, size_t len,
			    loff_t *ppos)
{
	loff_t pos = *ppos;
	size_t i;
	int word;

	if (!valid_range(pos, len))
		return -EINVAL;
	if (len == 0)
		return 0;
	for (i = 0; i < len; i += 4, pos += 4) {
		word = crspace_readl(pos);
		if (put_user(word, (int __user *)(to + i)) != 0)
			break;
	}
	*ppos = pos;
	return i ?: -EFAULT;
}

static ssize_t livefish_write(struct file *filp, const char __user *from,
			     size_t len, loff_t *ppos)
{
	loff_t pos = *ppos;
	size_t i;
	int word;

	if (!valid_range(pos, len))
		return -EINVAL;
	if (len == 0)
		return 0;
	for (i = 0; i < len; i += 4, pos += 4) {
		if (get_user(word, (int __user *)(from + i)) != 0)
			break;
		crspace_writel(word, pos);
	}
	*ppos = pos;
	return i ?: -EFAULT;
}

static const struct file_operations livefish_fops = {
	.owner		= THIS_MODULE,
	.llseek		= livefish_llseek,
	.read		= livefish_read,
	.write		= livefish_write,
};

/* This name causes the correct semantics for the Mellanox MST tools. */
static struct miscdevice livefish_dev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "bf-livefish",
	.mode           = 0600,
	.fops		= &livefish_fops
};

/* Release any VA or PA mappings that have been set up. */
static void livefish_cleanup_mappings(void)
{
	if (hca_va)
		iounmap(hca_va);
	if (hca_pa)
		release_mem_region(hca_pa, HCA_SIZE);
	if (tyu_va)
		iounmap(tyu_va);
	if (tyu_pa)
		release_mem_region(tyu_pa, TYU_SIZE);
}

static int livefish_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret = -EINVAL;

	/* Find and map the HCA region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -ENODEV;
	if (resource_size(res) < HCA_SIZE) {
		dev_warn(&pdev->dev, "HCA space too small: %#lx, not %#lx\n",
			 (long)resource_size(res), HCA_SIZE);
		return -EINVAL;
	}

	if (request_mem_region(res->start, HCA_SIZE, "LiveFish (HCA)") == NULL)
		return -EINVAL;
	hca_pa = res->start;
	hca_va = ioremap(res->start, HCA_SIZE);
	if (hca_va == NULL)
		goto err;

	/* Find and map the TYU efuse region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res == NULL)
		goto err;
	if (resource_size(res) < TYU_SIZE) {
		dev_warn(&pdev->dev, "TYU space too small: %#lx, not %#lx\n",
			 (long)resource_size(res), TYU_SIZE);
		goto err;
	}

	if (request_mem_region(res->start, TYU_SIZE, "LiveFish (TYU)") == NULL)
		goto err;
	tyu_pa = res->start;
	tyu_va = ioremap(res->start, TYU_SIZE);
	if (tyu_va == NULL)
		goto err;

	ret = misc_register(&livefish_dev);
	if (ret)
		goto err;

	dev_info(&pdev->dev, "probed\n");

	return 0;

err:
	livefish_cleanup_mappings();
	return ret;
}

static int livefish_remove(struct platform_device *pdev)
{
	misc_deregister(&livefish_dev);
	livefish_cleanup_mappings();
	return 0;
}

static const struct of_device_id livefish_of_match[] = {
	{ .compatible = "mellanox,mlxbf-livefish" },
	{},
};

MODULE_DEVICE_TABLE(of, livefish_of_match);

static const struct acpi_device_id livefish_acpi_match[] = {
	{ "MLNXBF05", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, livefish_acpi_match);

static struct platform_driver livefish_driver = {
	.driver	= {
		.name		= "mlxbf-livefish",
		.of_match_table	= livefish_of_match,
		.acpi_match_table = ACPI_PTR(livefish_acpi_match),
	},
	.probe	= livefish_probe,
	.remove	= livefish_remove,
};

module_platform_driver(livefish_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mellanox BlueField LiveFish driver");
MODULE_AUTHOR("Mellanox Technologies Inc.");
