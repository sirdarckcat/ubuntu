// SPDX-License-Identifier: GPL-2.0
/*
 *  Mellanox boot control driver
 *  This driver provides a sysfs interface for systems management
 *  software to manage reset-time actions.
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
 */

#include <linux/acpi.h>
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/if_ether.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "mlx-bootctl.h"

#define DRIVER_NAME		"mlx-bootctl"
#define DRIVER_VERSION		"1.3"
#define DRIVER_DESCRIPTION	"Mellanox boot control driver"

#define SB_MODE_SECURE_MASK	0x03
#define SB_MODE_TEST_MASK	0x0c

#define SB_KEY_NUM		4

struct boot_name {
	int value;
	const char name[12];
};

static struct boot_name boot_names[] = {
	{ MLNX_BOOT_EXTERNAL,		"external"	},
	{ MLNX_BOOT_EMMC,		"emmc"		},
	{ MLNX_BOOT_SWAP_EMMC,		"swap_emmc"	},
	{ MLNX_BOOT_EMMC_LEGACY,	"emmc_legacy"	},
	{ MLNX_BOOT_NONE,		"none"		},
	{ -1,				""		}
};

static char lifecycle_states[][16] = {
	[0] = "Production",
	[1] = "GA Secured",
	[2] = "GA Non-Secured",
	[3] = "RMA",
};

/* ctl/data register within the resource. */
#define RSH_SCRATCH_BUF_CTL_OFF		0
#define RSH_SCRATCH_BUF_DATA_OFF	0x10

static void __iomem *rsh_boot_data;
static void __iomem *rsh_boot_cnt;
static void __iomem *rsh_semaphore;
static void __iomem *rsh_scratch_buf_ctl;
static void __iomem *rsh_scratch_buf_data;

/*
 * Objects are stored within the MFG partition per type. Type 0 is not
 * supported.
 */
enum {
	MLNX_MFG_TYPE_OOB_MAC = 1,
	MLNX_MFG_TYPE_OPN_0,
	MLNX_MFG_TYPE_OPN_1,
};

/* This mutex is used to serialize MFG write and lock operations. */
static DEFINE_MUTEX(mfg_ops_lock);

#define MLNX_MFG_OPN_VAL_LEN       16
#define MLNX_MFG_OPN_VAL_WORD_CNT  (MLNX_MFG_OPN_VAL_LEN / 8)

#define MLNX_MFG_OOB_MAC_LEN       ETH_ALEN
/*
 * The MAC address consists of 6 bytes (2 digits each) separated by ':'.
 * The expected format is: "XX:XX:XX:XX:XX:XX"
 */
#define MLNX_MFG_OOB_MAC_FORMAT_LEN \
        ((MLNX_MFG_OOB_MAC_LEN * 2) + (MLNX_MFG_OOB_MAC_LEN - 1))

/* The SMC calls in question are atomic, so we don't have to lock here. */
static int smc_call1(unsigned int smc_op, int smc_arg)
{
	struct arm_smccc_res res;

	arm_smccc_smc(smc_op, smc_arg, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

/* Syntactic sugar to avoid having to specify an unused argument. */
#define smc_call0(smc_op) smc_call1(smc_op, 0)

static int reset_action_to_val(const char *action, size_t len)
{
	struct boot_name *bn;

	/* Accept string either with or without a newline terminator */
	if (action[len-1] == '\n')
		--len;

	for (bn = boot_names; bn->value >= 0; ++bn)
		if (strncmp(bn->name, action, len) == 0)
			break;

	return bn->value;
}

static const char *reset_action_to_string(int action)
{
	struct boot_name *bn;

	for (bn = boot_names; bn->value >= 0; ++bn)
		if (bn->value == action)
			break;

	return bn->name;
}

static ssize_t post_reset_wdog_show(struct device_driver *drv,
				    char *buf)
{
	return sprintf(buf, "%d\n", smc_call0(MLNX_GET_POST_RESET_WDOG));
}

static ssize_t post_reset_wdog_store(struct device_driver *drv,
				     const char *buf, size_t count)
{
	int err;
	unsigned long watchdog;

	err = kstrtoul(buf, 10, &watchdog);
	if (err)
		return err;

	if (smc_call1(MLNX_SET_POST_RESET_WDOG, watchdog) < 0)
		return -EINVAL;

	return count;
}

static ssize_t reset_action_show(struct device_driver *drv,
				 char *buf)
{
	return sprintf(buf, "%s\n", reset_action_to_string(
			       smc_call0(MLNX_GET_RESET_ACTION)));
}

static ssize_t reset_action_store(struct device_driver *drv,
				  const char *buf, size_t count)
{
	int action = reset_action_to_val(buf, count);

	if (action < 0 || action == MLNX_BOOT_NONE)
		return -EINVAL;

	if (smc_call1(MLNX_SET_RESET_ACTION, action) < 0)
		return -EINVAL;

	return count;
}

static ssize_t second_reset_action_show(struct device_driver *drv,
					char *buf)
{
	return sprintf(buf, "%s\n", reset_action_to_string(
			       smc_call0(MLNX_GET_SECOND_RESET_ACTION)));
}

static ssize_t second_reset_action_store(struct device_driver *drv,
					 const char *buf, size_t count)
{
	int action = reset_action_to_val(buf, count);

	if (action < 0)
		return -EINVAL;

	if (smc_call1(MLNX_SET_SECOND_RESET_ACTION, action) < 0)
		return -EINVAL;

	return count;
}

static ssize_t lifecycle_state_show(struct device_driver *drv,
				    char *buf)
{
	int lc_state = smc_call1(MLNX_GET_TBB_FUSE_STATUS,
				 MLNX_FUSE_STATUS_LIFECYCLE);

	if (lc_state < 0)
		return -EINVAL;

	lc_state &= (SB_MODE_TEST_MASK | SB_MODE_SECURE_MASK);
	/*
	 * If the test bits are set, we specify that the current state may be
	 * due to using the test bits.
	 */
	if ((lc_state & SB_MODE_TEST_MASK) != 0) {

		lc_state &= SB_MODE_SECURE_MASK;

		return sprintf(buf, "%s(test)\n", lifecycle_states[lc_state]);
	}

	return sprintf(buf, "%s\n", lifecycle_states[lc_state]);
}

static ssize_t secure_boot_fuse_state_show(struct device_driver *drv,
					   char *buf)
{
	int key;
	int buf_len = 0;
	int upper_key_used = 0;
	int sb_key_state = smc_call1(MLNX_GET_TBB_FUSE_STATUS,
				     MLNX_FUSE_STATUS_KEYS);

	if (sb_key_state < 0)
		return -EINVAL;

	for (key = SB_KEY_NUM - 1; key >= 0; key--) {
		int burnt = ((sb_key_state & (1 << key)) != 0);
		int valid = ((sb_key_state & (1 << (key + SB_KEY_NUM))) != 0);

		buf_len += sprintf(buf + buf_len, "Ver%d:", key);
		if (upper_key_used) {
			if (burnt) {
				if (valid)
					buf_len += sprintf(buf + buf_len,
							  "Used");
				else
					buf_len += sprintf(buf + buf_len,
							  "Wasted");
			} else {
				if (valid)
					buf_len += sprintf(buf + buf_len,
							  "Invalid");
				else
					buf_len += sprintf(buf + buf_len,
							  "Skipped");
			}
		} else {
			if (burnt) {
				if (valid) {
					upper_key_used = 1;
					buf_len += sprintf(buf + buf_len,
							  "In use");
				} else
					buf_len += sprintf(buf + buf_len,
							  "Burn incomplete");
			} else {
				if (valid)
					buf_len += sprintf(buf + buf_len,
							  "Invalid");
				else
					buf_len += sprintf(buf + buf_len,
							  "Free");
			}
		}
		buf_len += sprintf(buf + buf_len, "\n");
	}

	return buf_len;
}

static ssize_t fw_reset_store(struct device_driver *drv,
			      const char *buf, size_t count)
{
	int err;
	unsigned long key;

	err = kstrtoul(buf, 16, &key);
	if (err)
		return err;

	if (smc_call1(MLNX_HANDLE_FW_RESET, key) < 0)
		return -EINVAL;

	return count;
}

static ssize_t oob_mac_show(struct device_driver *drv, char *buf)
{
	char mac_str[MLNX_MFG_OOB_MAC_FORMAT_LEN] = { 0 };
	struct arm_smccc_res res;
	u8 *mac_byte_ptr;

	arm_smccc_smc(MLNX_HANDLE_GET_MFG_INFO, MLNX_MFG_TYPE_OOB_MAC, 0, 0, 0,
		      0, 0, 0, &res);
	if (res.a0)
		return -EPERM;

	mac_byte_ptr = (u8 *)&res.a1;

	sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
		mac_byte_ptr[0], mac_byte_ptr[1], mac_byte_ptr[2],
		mac_byte_ptr[3], mac_byte_ptr[4], mac_byte_ptr[5]);

	return sprintf(buf, "%s\n", mac_str);
}

static ssize_t oob_mac_store(struct device_driver *drv, const char *buf,
			     size_t count)
{
	int byte[MLNX_MFG_OOB_MAC_FORMAT_LEN] = { 0 };
	struct arm_smccc_res res;
	u64 mac_addr = 0;
	u8 *mac_byte_ptr;
	int byte_idx;

	if ((count - 1) != MLNX_MFG_OOB_MAC_FORMAT_LEN)
		return -EINVAL;

	if (MLNX_MFG_OOB_MAC_LEN != sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
					   &byte[0], &byte[1], &byte[2],
					   &byte[3], &byte[4], &byte[5]))
		return -EINVAL;

	mac_byte_ptr = (u8 *)&mac_addr;

	for (byte_idx = 0; byte_idx < MLNX_MFG_OOB_MAC_LEN; byte_idx++)
		mac_byte_ptr[byte_idx] = (u8) byte[byte_idx];

	mutex_lock(&mfg_ops_lock);
	arm_smccc_smc(MLNX_HANDLE_SET_MFG_INFO, MLNX_MFG_TYPE_OOB_MAC,
		  MLNX_MFG_OOB_MAC_LEN, mac_addr, 0, 0, 0, 0, &res);
	mutex_unlock(&mfg_ops_lock);

	return res.a0 ? -EPERM : count;
}

static u8 get_opn_type(u8 word)
{
	switch(word) {
	case 0:
		return MLNX_MFG_TYPE_OPN_0;
	case 1:
		return MLNX_MFG_TYPE_OPN_1;
	}

	return 0;
}

static int get_opn_data(u64 *data, u8 word)
{
	struct arm_smccc_res res;
	u8 type;

	type = get_opn_type(word);
	if (!type || !data)
		return -EINVAL;

	arm_smccc_smc(MLNX_HANDLE_GET_MFG_INFO, type, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		return -EPERM;

	*data = res.a1;

	return 0;
}

static int set_opn_data(u64 data, u8 word)
{
	struct arm_smccc_res res;
	u8 type;

	type = get_opn_type(word);
	if (!type)
		return -EINVAL;

	arm_smccc_smc(MLNX_HANDLE_SET_MFG_INFO, type, sizeof(u64), data, 0, 0,
		      0, 0, &res);
	if (res.a0)
		return -EPERM;

	return 0;
}

static ssize_t opn_str_show(struct device_driver *drv, char *buf)
{
	u64 opn_data[MLNX_MFG_OPN_VAL_WORD_CNT] = { 0 };
	char opn_str[MLNX_MFG_OPN_VAL_LEN] = { 0 };
	int word, err;

	for (word = 0; word < MLNX_MFG_OPN_VAL_WORD_CNT; word++) {
		err = get_opn_data(&opn_data[word], word);
		if (err)
			return err;
	}

	memcpy(opn_str, opn_data, MLNX_MFG_OPN_VAL_LEN);

	return sprintf(buf, "%s", opn_str);
}

static ssize_t opn_str_store(struct device_driver *drv, const char *buf,
			     size_t count)
{
	u64 opn[MLNX_MFG_OPN_VAL_WORD_CNT] = { 0 };
	int word, err;

	if (count > MLNX_MFG_OPN_VAL_LEN)
		return -EINVAL;

	memcpy(opn, buf, strlen(buf));

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_OPN_VAL_WORD_CNT; word++) {
		err = set_opn_data(opn[word], word);
		if (err) {
			mutex_unlock(&mfg_ops_lock);
			return err;
		}
	}
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static ssize_t mfg_lock_store(struct device_driver *drv, const char *buf,
			      size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val != 1)
		return -EINVAL;

	mutex_lock(&mfg_ops_lock);
	smc_call0(MLNX_HANDLE_LOCK_MFG_INFO);
	mutex_unlock(&mfg_ops_lock);

	return count;
}

/* Log header format. */
#define RSH_LOG_TYPE_SHIFT	56
#define RSH_LOG_LEN_SHIFT	48
#define RSH_LOG_LEVEL_SHIFT	0

/* Module ID and type used here. */
#define RSH_LOG_TYPE		0x04ULL	/* message */

/* Log message level. */
enum {
	RSH_LOG_INFO,
	RSH_LOG_WARN,
	RSH_LOG_ERR
};

const char *rsh_log_level[] = {"INFO", "WARN", "ERR"};

/* Size(8-byte words) of the log buffer. */
#define RSH_SCRATCH_BUF_CTL_IDX_MAX	0x7f

static ssize_t rsh_log_store(struct device_driver *drv, const char *buf,
			     size_t count)
{
	int idx, num, len, size = (int)count, level = RSH_LOG_INFO;
	unsigned long timeout;
	u64 data;

	if (!size)
		return -EINVAL;

	if (!rsh_semaphore || !rsh_scratch_buf_ctl)
		return -EOPNOTSUPP;

	/* Ignore line break at the end. */
	if (buf[size-1] == 0xa)
		size--;

	/* Check the message prefix. */
	for (idx = 0; idx < ARRAY_SIZE(rsh_log_level); idx++) {
		len = strlen(rsh_log_level[idx]);
		if (len + 1 < size && !strncmp(buf, rsh_log_level[idx], len)) {
			buf += len + 1;
			size -= len + 1;
			level = idx;
			break;
		}
	}

	/* Ignore leading spaces. */
	while (size > 0 && buf[0] == ' ') {
		size--;
		buf++;
	}

	/* Take the semaphore. */
	timeout = jiffies + msecs_to_jiffies(100);
	while (readq(rsh_semaphore)) {
		if (time_after(jiffies, timeout))
			return -ETIMEDOUT;
	}

	/* Calculate how many words are available. */
	num = (size + sizeof(u64) - 1) / sizeof(u64);
	idx = readq(rsh_scratch_buf_ctl);
	if (idx + num + 1 >= RSH_SCRATCH_BUF_CTL_IDX_MAX)
		num = RSH_SCRATCH_BUF_CTL_IDX_MAX - idx - 1;
	if (num <= 0)
		goto done;

	/* Write Header. */
	data = (RSH_LOG_TYPE << RSH_LOG_TYPE_SHIFT) |
		((u64)num << RSH_LOG_LEN_SHIFT) |
		((u64)level << RSH_LOG_LEVEL_SHIFT);
	writeq(data, rsh_scratch_buf_data);

	/* Write message. */
	for (idx = 0, len = size; idx < num && len > 0; idx++) {
		if (len <= sizeof(u64)) {
			data = 0;
			memcpy(&data, buf, len);
			len = 0;
		} else {
			memcpy (&data, buf, sizeof(u64));
			len -= sizeof(u64);
			buf += sizeof(u64);
		}
		writeq(data, rsh_scratch_buf_data);
	}

done:
	/* Release the semaphore. */
	writeq(0, rsh_semaphore);

	/* Ignore the rest if no more space. */
	return count;
}

#define MBC_DRV_ATTR(_name) DRIVER_ATTR_RW(_name)

static MBC_DRV_ATTR(post_reset_wdog);
static MBC_DRV_ATTR(reset_action);
static MBC_DRV_ATTR(second_reset_action);
static DRIVER_ATTR_RO(lifecycle_state);
static DRIVER_ATTR_RO(secure_boot_fuse_state);
static DRIVER_ATTR_WO(fw_reset);
static MBC_DRV_ATTR(oob_mac);
static MBC_DRV_ATTR(opn_str);
static DRIVER_ATTR_WO(mfg_lock);
static DRIVER_ATTR_WO(rsh_log);

static struct attribute *mbc_dev_attrs[] = {
	&driver_attr_post_reset_wdog.attr,
	&driver_attr_reset_action.attr,
	&driver_attr_second_reset_action.attr,
	&driver_attr_lifecycle_state.attr,
	&driver_attr_secure_boot_fuse_state.attr,
	&driver_attr_fw_reset.attr,
	&driver_attr_oob_mac.attr,
	&driver_attr_opn_str.attr,
	&driver_attr_mfg_lock.attr,
	&driver_attr_rsh_log.attr,
	NULL
};

static struct attribute_group mbc_attr_group = {
	.attrs = mbc_dev_attrs
};

static const struct attribute_group *mbc_attr_groups[] = {
	&mbc_attr_group,
	NULL
};

static const struct of_device_id mbc_dt_ids[] = {
	{.compatible = "mellanox,bootctl"},
	{},
};

MODULE_DEVICE_TABLE(of, mbc_dt_ids);

static const struct acpi_device_id mbc_acpi_ids[] = {
	{"MLNXBF04", 0},
	{},
};

MODULE_DEVICE_TABLE(acpi, mbc_acpi_ids);

static ssize_t mbc_bootfifo_read_raw(struct file *filp, struct kobject *kobj,
				     struct bin_attribute *bin_attr,
				     char *buf, loff_t pos, size_t count)
{
	unsigned long timeout = jiffies + HZ / 2;
	char *p = buf;
	int cnt = 0;
	u64 data;

	/* Give up reading if no more data within 500ms. */
	while (count >= sizeof(data)) {
		if (!cnt) {
			cnt = readq(rsh_boot_cnt);
			if (!cnt) {
				if (time_after(jiffies, timeout))
					break;
				udelay(10);
				continue;
			}
		}

		data = readq(rsh_boot_data);
		memcpy(p, &data, sizeof(data));
		count -= sizeof(data);
		p += sizeof(data);
		cnt--;
		timeout = jiffies + HZ / 2;
	}

	return p - buf;
}

static struct bin_attribute mbc_bootfifo_sysfs_attr = {
	.attr = { .name = "bootfifo", .mode = S_IRUSR },
	.read = mbc_bootfifo_read_raw,
};

static int mbc_probe(struct platform_device *pdev)
{
	struct resource *resource;
	struct arm_smccc_res res;
	void __iomem *data;
	int err;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource)
		return -ENODEV;
	rsh_boot_data = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(rsh_boot_data))
		return PTR_ERR(rsh_boot_data);

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!resource)
		return -ENODEV;
	rsh_boot_cnt = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(rsh_boot_cnt))
		return PTR_ERR(rsh_boot_cnt);

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (resource) {
		data = devm_ioremap_resource(&pdev->dev, resource);
		if (!IS_ERR(data))
			rsh_semaphore = data;
	}

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (resource) {
		data = devm_ioremap_resource(&pdev->dev, resource);
		if (!IS_ERR(data)) {
			rsh_scratch_buf_ctl = data + RSH_SCRATCH_BUF_CTL_OFF;
			rsh_scratch_buf_data = data + RSH_SCRATCH_BUF_DATA_OFF;
		}
	}

	/*
	 * Ensure we have the UUID we expect for this service.
	 * Note that the functionality we want is present in the first
	 * released version of this service, so we don't check the version.
	 */
	arm_smccc_smc(MLNX_SIP_SVC_UID, 0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 != 0x89c036b4 || res.a1 != 0x11e6e7d7 ||
	    res.a2 != 0x1a009787 || res.a3 != 0xc4bf00ca)
		return -ENODEV;

	/*
	 * When watchdog is used, it sets the boot mode to MLNX_BOOT_SWAP_EMMC
	 * in case of boot failures. However it doesn't clear the state if there
	 * is no failure. Restore the default boot mode here to avoid any
	 * unnecessary boot partition swapping.
	 */
	if (smc_call1(MLNX_SET_RESET_ACTION, MLNX_BOOT_EMMC) < 0)
		pr_err("Unable to reset the EMMC boot mode\n");

	err = sysfs_create_bin_file(&pdev->dev.kobj, &mbc_bootfifo_sysfs_attr);
	if (err) {
		pr_err("Unable to create bootfifo sysfs file, error %d\n", err);
		return err;
	}

	pr_info("%s (version %s)\n", DRIVER_DESCRIPTION, DRIVER_VERSION);

	return 0;
}

static int mbc_remove(struct platform_device *pdev)
{
	sysfs_remove_bin_file(&pdev->dev.kobj, &mbc_bootfifo_sysfs_attr);

	return 0;
}

static struct platform_driver mbc_driver = {
	.probe = mbc_probe,
	.remove = mbc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.groups = mbc_attr_groups,
		.of_match_table = mbc_dt_ids,
		.acpi_match_table = ACPI_PTR(mbc_acpi_ids),
	}
};

module_platform_driver(mbc_driver);

MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR("Mellanox Technologies");
MODULE_LICENSE("GPL");
