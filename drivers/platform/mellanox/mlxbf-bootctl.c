// SPDX-License-Identifier: GPL-2.0+
/*
 * Mellanox boot control driver
 *
 * This driver provides a sysfs interface for systems management
 * software to manage reset-time actions.
 *
 * Copyright (C) 2019 Mellanox Technologies
 */

#include <linux/acpi.h>
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/if_ether.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "mlxbf-bootctl.h"

#define MLXBF_BOOTCTL_SB_SECURE_MASK		0x03
#define MLXBF_BOOTCTL_SB_TEST_MASK		0x0c
#define MLXBF_BOOTCTL_SB_DEV_MASK		BIT(4)

#define MLXBF_SB_KEY_NUM			4

/* UUID used to probe ATF service. */
static const char *mlxbf_bootctl_svc_uuid_str =
	"89c036b4-e7d7-11e6-8797-001aca00bfc4";

struct mlxbf_bootctl_name {
	u32 value;
	const char *name;
};

static struct mlxbf_bootctl_name boot_names[] = {
	{ MLXBF_BOOTCTL_EXTERNAL, "external" },
	{ MLXBF_BOOTCTL_EMMC, "emmc" },
	{ MLNX_BOOTCTL_SWAP_EMMC, "swap_emmc" },
	{ MLXBF_BOOTCTL_EMMC_LEGACY, "emmc_legacy" },
	{ MLXBF_BOOTCTL_NONE, "none" },
};

enum {
	MLXBF_BOOTCTL_SB_LIFECYCLE_PRODUCTION = 0,
	MLXBF_BOOTCTL_SB_LIFECYCLE_GA_SECURE = 1,
	MLXBF_BOOTCTL_SB_LIFECYCLE_GA_NON_SECURE = 2,
	MLXBF_BOOTCTL_SB_LIFECYCLE_RMA = 3
};

static const char * const mlxbf_bootctl_lifecycle_states[] = {
	[MLXBF_BOOTCTL_SB_LIFECYCLE_PRODUCTION] = "Production",
	[MLXBF_BOOTCTL_SB_LIFECYCLE_GA_SECURE] = "GA Secured",
	[MLXBF_BOOTCTL_SB_LIFECYCLE_GA_NON_SECURE] = "GA Non-Secured",
	[MLXBF_BOOTCTL_SB_LIFECYCLE_RMA] = "RMA",
};

/* Log header format. */
#define MLXBF_RSH_LOG_TYPE_MASK		GENMASK_ULL(59, 56)
#define MLXBF_RSH_LOG_LEN_MASK		GENMASK_ULL(54, 48)
#define MLXBF_RSH_LOG_LEVEL_MASK	GENMASK_ULL(7, 0)

/* Log module ID and type (only MSG type in Linux driver for now). */
#define MLXBF_RSH_LOG_TYPE_MSG		0x04ULL

/* Log ctl/data register offset. */
#define MLXBF_RSH_SCRATCH_BUF_CTL_OFF	0
#define MLXBF_RSH_SCRATCH_BUF_DATA_OFF	0x10

static int rsh_log_clear_on_read;
module_param(rsh_log_clear_on_read, int, 0644);
MODULE_PARM_DESC(rsh_log_clear_on_read, "Clear rshim logging buffer after read.");

/* Log header format. */
#define RSH_LOG_TYPE_SHIFT	56
#define RSH_LOG_LEN_SHIFT	48
#define RSH_LOG_LEVEL_SHIFT	0

/* Module ID and type used here. */
#define BF_RSH_LOG_TYPE_UNKNOWN		0x00ULL
#define BF_RSH_LOG_TYPE_PANIC		0x01ULL
#define BF_RSH_LOG_TYPE_EXCEPTION	0x02ULL
#define BF_RSH_LOG_TYPE_UNUSED		0x03ULL
#define BF_RSH_LOG_TYPE_MSG		0x04ULL

/* Utility macro. */
#define BF_RSH_LOG_MOD_MASK		0x0FULL
#define BF_RSH_LOG_MOD_SHIFT		60
#define BF_RSH_LOG_TYPE_MASK		0x0FULL
#define BF_RSH_LOG_TYPE_SHIFT		56
#define BF_RSH_LOG_LEN_MASK		0x7FULL
#define BF_RSH_LOG_LEN_SHIFT		48
#define BF_RSH_LOG_ARG_MASK		0xFFFFFFFFULL
#define BF_RSH_LOG_ARG_SHIFT		16
#define BF_RSH_LOG_HAS_ARG_MASK		0xFFULL
#define BF_RSH_LOG_HAS_ARG_SHIFT	8
#define BF_RSH_LOG_LEVEL_MASK		0xFFULL
#define BF_RSH_LOG_LEVEL_SHIFT		0
#define BF_RSH_LOG_PC_MASK		0xFFFFFFFFULL
#define BF_RSH_LOG_PC_SHIFT		0
#define BF_RSH_LOG_SYNDROME_MASK	0xFFFFFFFFULL
#define BF_RSH_LOG_SYNDROME_SHIFT	0

#define BF_RSH_LOG_HEADER_GET(f, h) \
	(((h) >> BF_RSH_LOG_##f##_SHIFT) & BF_RSH_LOG_##f##_MASK)

/* Log module */
const char * const mlxbf_rsh_log_mod[] = {
	"MISC", "BL1", "BL2", "BL2R", "BL31", "UEFI"
};

#define AARCH64_MRS_REG_SHIFT 5
#define AARCH64_MRS_REG_MASK  0xffff
#define AARCH64_ESR_ELX_EXCEPTION_CLASS_SHIFT 26

struct rsh_log_reg {
	char *name;
	u32 opcode;
} rsh_log_reg;

static struct rsh_log_reg rsh_log_regs[] = {
	{"actlr_el1",		0b1100000010000001},
	{"actlr_el2",		0b1110000010000001},
	{"actlr_el3",		0b1111000010000001},
	{"afsr0_el1",		0b1100001010001000},
	{"afsr0_el2",		0b1110001010001000},
	{"afsr0_el3",		0b1111001010001000},
	{"afsr1_el1",		0b1100001010001001},
	{"afsr1_el2",		0b1110001010001001},
	{"afsr1_el3",		0b1111001010001001},
	{"amair_el1",		0b1100010100011000},
	{"amair_el2",		0b1110010100011000},
	{"amair_el3",		0b1111010100011000},
	{"ccsidr_el1",		0b1100100000000000},
	{"clidr_el1",		0b1100100000000001},
	{"cntkctl_el1",		0b1100011100001000},
	{"cntp_ctl_el0",	0b1101111100010001},
	{"cntp_cval_el0",	0b1101111100010010},
	{"cntv_ctl_el0",	0b1101111100011001},
	{"cntv_cval_el0",	0b1101111100011010},
	{"contextidr_el1",	0b1100011010000001},
	{"cpacr_el1",		0b1100000010000010},
	{"cptr_el2",		0b1110000010001010},
	{"cptr_el3",		0b1111000010001010},
	{"vtcr_el2",		0b1110000100001010},
	{"ctr_el0",		0b1101100000000001},
	{"currentel",		0b1100001000010010},
	{"dacr32_el2",		0b1110000110000000},
	{"daif",		0b1101101000010001},
	{"dczid_el0",		0b1101100000000111},
	{"dlr_el0",		0b1101101000101001},
	{"dspsr_el0",		0b1101101000101000},
	{"elr_el1",		0b1100001000000001},
	{"elr_el2",		0b1110001000000001},
	{"elr_el3",		0b1111001000000001},
	{"esr_el1",		0b1100001010010000},
	{"esr_el2",		0b1110001010010000},
	{"esr_el3",		0b1111001010010000},
	{"esselr_el1",		0b1101000000000000},
	{"far_el1",		0b1100001100000000},
	{"far_el2",		0b1110001100000000},
	{"far_el3",		0b1111001100000000},
	{"fpcr",		0b1101101000100000},
	{"fpexc32_el2",		0b1110001010011000},
	{"fpsr",		0b1101101000100001},
	{"hacr_el2",		0b1110000010001111},
	{"har_el2",		0b1110000010001000},
	{"hpfar_el2",		0b1110001100000100},
	{"hstr_el2",		0b1110000010001011},
	{"far_el1",		0b1100001100000000},
	{"far_el2",		0b1110001100000000},
	{"far_el3",		0b1111001100000000},
	{"hcr_el2",		0b1110000010001000},
	{"hpfar_el2",		0b1110001100000100},
	{"id_aa64afr0_el1",	0b1100000000101100},
	{"id_aa64afr1_el1",	0b1100000000101101},
	{"id_aa64dfr0_el1",	0b1100000000101100},
	{"id_aa64isar0_el1",	0b1100000000110000},
	{"id_aa64isar1_el1",	0b1100000000110001},
	{"id_aa64mmfr0_el1",	0b1100000000111000},
	{"id_aa64mmfr1_el1",	0b1100000000111001},
	{"id_aa64pfr0_el1",	0b1100000000100000},
	{"id_aa64pfr1_el1",	0b1100000000100001},
	{"ifsr32_el2",		0b1110001010000001},
	{"isr_el1",		0b1100011000001000},
	{"mair_el1",		0b1100010100010000},
	{"mair_el2",		0b1110010100010000},
	{"mair_el3",		0b1111010100010000},
	{"midr_el1",		0b1100000000000000},
	{"mpidr_el1",		0b1100000000000101},
	{"nzcv",		0b1101101000010000},
	{"revidr_el1",		0b1100000000000110},
	{"rmr_el3",		0b1111011000000010},
	{"par_el1",		0b1100001110100000},
	{"rvbar_el3",		0b1111011000000001},
	{"scr_el3",		0b1111000010001000},
	{"sctlr_el1",		0b1100000010000000},
	{"sctlr_el2",		0b1110000010000000},
	{"sctlr_el3",		0b1111000010000000},
	{"sp_el0",		0b1100001000001000},
	{"sp_el1",		0b1110001000001000},
	{"spsel",		0b1100001000010000},
	{"spsr_abt",		0b1110001000011001},
	{"spsr_el1",		0b1100001000000000},
	{"spsr_el2",		0b1110001000000000},
	{"spsr_el3",		0b1111001000000000},
	{"spsr_fiq",		0b1110001000011011},
	{"spsr_irq",		0b1110001000011000},
	{"spsr_und",		0b1110001000011010},
	{"tcr_el1",		0b1100000100000010},
	{"tcr_el2",		0b1110000100000010},
	{"tcr_el3",		0b1111000100000010},
	{"tpidr_el0",		0b1101111010000010},
	{"tpidr_el1",		0b1100011010000100},
	{"tpidr_el2",		0b1110011010000010},
	{"tpidr_el3",		0b1111011010000010},
	{"tpidpro_el0",		0b1101111010000011},
	{"vbar_el1",		0b1100011000000000},
	{"vbar_el2",		0b1110011000000000},
	{"vbar_el3",		0b1111011000000000},
	{"vmpidr_el2",		0b1110000000000101},
	{"vpidr_el2",		0b1110000000000000},
	{"ttbr0_el1",		0b1100000100000000},
	{"ttbr0_el2",		0b1110000100000000},
	{"ttbr0_el3",		0b1111000100000000},
	{"ttbr1_el1",		0b1100000100000001},
	{"vtcr_el2",		0b1110000100001010},
	{"vttbr_el2",		0b1110000100001000},
	{NULL,			0b0000000000000000},
};

/* Log message levels. */
enum {
	MLXBF_RSH_LOG_INFO,
	MLXBF_RSH_LOG_WARN,
	MLXBF_RSH_LOG_ERR,
	MLXBF_RSH_LOG_ASSERT
};

/* Mapped pointer for RSH_BOOT_FIFO_DATA and RSH_BOOT_FIFO_COUNT register. */
static void __iomem *mlxbf_rsh_boot_data;
static void __iomem *mlxbf_rsh_boot_cnt;

/* Mapped pointer for rsh log semaphore/ctrl/data register. */
static void __iomem *mlxbf_rsh_semaphore;
static void __iomem *mlxbf_rsh_scratch_buf_ctl;
static void __iomem *mlxbf_rsh_scratch_buf_data;

/* Rsh log levels. */
static const char * const mlxbf_rsh_log_level[] = {
	"INFO", "WARN", "ERR", "ASSERT"};

static DEFINE_MUTEX(icm_ops_lock);
static DEFINE_MUTEX(os_up_lock);
static DEFINE_MUTEX(mfg_ops_lock);

/*
 * Objects are stored within the MFG partition per type.
 * Type 0 is not supported.
 */
enum {
	MLNX_MFG_TYPE_OOB_MAC = 1,
	MLNX_MFG_TYPE_OPN_0,
	MLNX_MFG_TYPE_OPN_1,
	MLNX_MFG_TYPE_OPN_2,
	MLNX_MFG_TYPE_SKU_0,
	MLNX_MFG_TYPE_SKU_1,
	MLNX_MFG_TYPE_SKU_2,
	MLNX_MFG_TYPE_MODL_0,
	MLNX_MFG_TYPE_MODL_1,
	MLNX_MFG_TYPE_MODL_2,
	MLNX_MFG_TYPE_SN_0,
	MLNX_MFG_TYPE_SN_1,
	MLNX_MFG_TYPE_SN_2,
	MLNX_MFG_TYPE_UUID_0,
	MLNX_MFG_TYPE_UUID_1,
	MLNX_MFG_TYPE_UUID_2,
	MLNX_MFG_TYPE_UUID_3,
	MLNX_MFG_TYPE_UUID_4,
	MLNX_MFG_TYPE_REV,
};

#define MLNX_MFG_OPN_VAL_LEN         24
#define MLNX_MFG_SKU_VAL_LEN         24
#define MLNX_MFG_MODL_VAL_LEN        24
#define MLNX_MFG_SN_VAL_LEN          24
#define MLNX_MFG_UUID_VAL_LEN        40
#define MLNX_MFG_REV_VAL_LEN         8
#define MLNX_MFG_VAL_QWORD_CNT(type) \
	(MLNX_MFG_##type##_VAL_LEN / sizeof(u64))

/*
 * The MAC address consists of 6 bytes (2 digits each) separated by ':'.
 * The expected format is: "XX:XX:XX:XX:XX:XX"
 */
#define MLNX_MFG_OOB_MAC_FORMAT_LEN \
	((ETH_ALEN * 2) + (ETH_ALEN - 1))

/* ARM SMC call which is atomic and no need for lock. */
static int mlxbf_bootctl_smc(unsigned int smc_op, int smc_arg)
{
	struct arm_smccc_res res;

	arm_smccc_smc(smc_op, smc_arg, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

/* Return the action in integer or an error code. */
static int mlxbf_bootctl_reset_action_to_val(const char *action)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(boot_names); i++)
		if (sysfs_streq(boot_names[i].name, action))
			return boot_names[i].value;

	return -EINVAL;
}

/* Return the action in string. */
static const char *mlxbf_bootctl_action_to_string(int action)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(boot_names); i++)
		if (boot_names[i].value == action)
			return boot_names[i].name;

	return "invalid action";
}

static ssize_t post_reset_wdog_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int ret;

	ret = mlxbf_bootctl_smc(MLXBF_BOOTCTL_GET_POST_RESET_WDOG, 0);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", ret);
}

static ssize_t post_reset_wdog_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned long value;
	int ret;

	ret = kstrtoul(buf, 10, &value);
	if (ret)
		return ret;

	ret = mlxbf_bootctl_smc(MLXBF_BOOTCTL_SET_POST_RESET_WDOG, value);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t mlxbf_bootctl_show(int smc_op, char *buf)
{
	int action;

	action = mlxbf_bootctl_smc(smc_op, 0);
	if (action < 0)
		return action;

	return sprintf(buf, "%s\n", mlxbf_bootctl_action_to_string(action));
}

static int mlxbf_bootctl_store(int smc_op, const char *buf, size_t count)
{
	int ret, action;

	action = mlxbf_bootctl_reset_action_to_val(buf);
	if (action < 0)
		return action;

	ret = mlxbf_bootctl_smc(smc_op, action);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t reset_action_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return mlxbf_bootctl_show(MLXBF_BOOTCTL_GET_RESET_ACTION, buf);
}

static ssize_t reset_action_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return mlxbf_bootctl_store(MLXBF_BOOTCTL_SET_RESET_ACTION, buf, count);
}

static ssize_t second_reset_action_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return mlxbf_bootctl_show(MLXBF_BOOTCTL_GET_SECOND_RESET_ACTION, buf);
}

static ssize_t second_reset_action_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	return mlxbf_bootctl_store(MLXBF_BOOTCTL_SET_SECOND_RESET_ACTION, buf,
				   count);
}

static ssize_t lifecycle_state_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int status_bits;
	int use_dev_key;
	int test_state;
	int lc_state;

	status_bits = mlxbf_bootctl_smc(MLXBF_BOOTCTL_GET_TBB_FUSE_STATUS,
					MLXBF_BOOTCTL_FUSE_STATUS_LIFECYCLE);
	if (status_bits < 0)
		return status_bits;

	use_dev_key = status_bits & MLXBF_BOOTCTL_SB_DEV_MASK;
	test_state = status_bits & MLXBF_BOOTCTL_SB_TEST_MASK;
	lc_state = status_bits & MLXBF_BOOTCTL_SB_SECURE_MASK;

	/*
	 * If the test bits are set, we specify that the current state may be
	 * due to using the test bits.
	 */
	if (test_state) {
		return sprintf(buf, "%s(test)\n",
			       mlxbf_bootctl_lifecycle_states[lc_state]);
	} else if (use_dev_key &&
		   (lc_state == MLXBF_BOOTCTL_SB_LIFECYCLE_GA_SECURE)) {
		return sprintf(buf, "Secured (development)\n");
	}

	return sprintf(buf, "%s\n", mlxbf_bootctl_lifecycle_states[lc_state]);
}

static ssize_t secure_boot_fuse_state_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	int burnt, valid, key, key_state, buf_len = 0, upper_key_used = 0;
	const char *status;

	key_state = mlxbf_bootctl_smc(MLXBF_BOOTCTL_GET_TBB_FUSE_STATUS,
				      MLXBF_BOOTCTL_FUSE_STATUS_KEYS);
	if (key_state < 0)
		return key_state;

	/*
	 * key_state contains the bits for 4 Key versions, loaded from eFuses
	 * after a hard reset. Lower 4 bits are a thermometer code indicating
	 * key programming has started for key n (0000 = none, 0001 = version 0,
	 * 0011 = version 1, 0111 = version 2, 1111 = version 3). Upper 4 bits
	 * are a thermometer code indicating key programming has completed for
	 * key n (same encodings as the start bits). This allows for detection
	 * of an interruption in the programming process which has left the key
	 * partially programmed (and thus invalid). The process is to burn the
	 * eFuse for the new key start bit, burn the key eFuses, then burn the
	 * eFuse for the new key complete bit.
	 *
	 * For example 0000_0000: no key valid, 0001_0001: key version 0 valid,
	 * 0011_0011: key 1 version valid, 0011_0111: key version 2 started
	 * programming but did not complete, etc. The most recent key for which
	 * both start and complete bit is set is loaded. On soft reset, this
	 * register is not modified.
	 */
	for (key = MLXBF_SB_KEY_NUM - 1; key >= 0; key--) {
		burnt = key_state & BIT(key);
		valid = key_state & BIT(key + MLXBF_SB_KEY_NUM);

		if (burnt && valid)
			upper_key_used = 1;

		if (upper_key_used) {
			if (burnt)
				status = valid ? "Used" : "Wasted";
			else
				status = valid ? "Invalid" : "Skipped";
		} else {
			if (burnt)
				status = valid ? "InUse" : "Incomplete";
			else
				status = valid ? "Invalid" : "Free";
		}
		buf_len += sprintf(buf + buf_len, "%d:%s ", key, status);
	}
	buf_len += sprintf(buf + buf_len, "\n");

	return buf_len;
}

static ssize_t fw_reset_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned long key;
	int err;

	err = kstrtoul(buf, 16, &key);
	if (err)
		return err;

	if (mlxbf_bootctl_smc(MLXBF_BOOTCTL_FW_RESET, key) < 0)
		return -EINVAL;

	return count;
}

/* Size(8-byte words) of the log buffer. */
#define RSH_SCRATCH_BUF_CTL_IDX_MASK	0x7f

/* 100ms timeout */
#define RSH_SCRATCH_BUF_POLL_TIMEOUT	100000

static int mlxbf_rsh_log_sem_lock(void)
{
	unsigned long reg;

	return readq_poll_timeout(mlxbf_rsh_semaphore, reg, !reg, 0,
				  RSH_SCRATCH_BUF_POLL_TIMEOUT);
}

static void mlxbf_rsh_log_sem_unlock(void)
{
	writeq(0, mlxbf_rsh_semaphore);
}

static ssize_t rsh_log_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int rc, idx, num, len, level = MLXBF_RSH_LOG_INFO;
	size_t size = count;
	u64 data;

	if (!size)
		return -EINVAL;

	if (!mlxbf_rsh_semaphore || !mlxbf_rsh_scratch_buf_ctl)
		return -EOPNOTSUPP;

	/* Ignore line break at the end. */
	if (buf[size - 1] == '\n')
		size--;

	/* Check the message prefix. */
	for (idx = 0; idx < ARRAY_SIZE(mlxbf_rsh_log_level); idx++) {
		len = strlen(mlxbf_rsh_log_level[idx]);
		if (len + 1 < size &&
		    !strncmp(buf, mlxbf_rsh_log_level[idx], len)) {
			buf += len;
			size -= len;
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
	rc = mlxbf_rsh_log_sem_lock();
	if (rc)
		return rc;

	/* Calculate how many words are available. */
	idx = readq(mlxbf_rsh_scratch_buf_ctl);
	num = min((int)DIV_ROUND_UP(size, sizeof(u64)),
		  RSH_SCRATCH_BUF_CTL_IDX_MASK - idx - 1);
	if (num <= 0)
		goto done;

	/* Write Header. */
	data = FIELD_PREP(MLXBF_RSH_LOG_TYPE_MASK, MLXBF_RSH_LOG_TYPE_MSG);
	data |= FIELD_PREP(MLXBF_RSH_LOG_LEN_MASK, num);
	data |= FIELD_PREP(MLXBF_RSH_LOG_LEVEL_MASK, level);
	writeq(data, mlxbf_rsh_scratch_buf_data);

	/* Write message. */
	for (idx = 0; idx < num && size > 0; idx++) {
		if (size < sizeof(u64)) {
			data = 0;
			memcpy(&data, buf, size);
			size = 0;
		} else {
			memcpy(&data, buf, sizeof(u64));
			size -= sizeof(u64);
			buf += sizeof(u64);
		}
		writeq(data, mlxbf_rsh_scratch_buf_data);
	}

done:
	/* Release the semaphore. */
	mlxbf_rsh_log_sem_unlock();

	/* Ignore the rest if no more space. */
	return count;
}

static ssize_t large_icm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct arm_smccc_res res;

	mutex_lock(&icm_ops_lock);
	arm_smccc_smc(MLNX_HANDLE_GET_ICM_INFO, 0, 0, 0, 0,
		      0, 0, 0, &res);
	mutex_unlock(&icm_ops_lock);
	if (res.a0)
		return -EPERM;

	return snprintf(buf, PAGE_SIZE, "0x%lx", res.a1);
}

static ssize_t large_icm_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct arm_smccc_res res;
	unsigned long icm_data;
	int err;

	err = kstrtoul(buf, MLXBF_LARGE_ICMC_MAX_STRING_SIZE, &icm_data);
	if (err)
		return err;

	if ((icm_data != 0 && icm_data < MLXBF_LARGE_ICMC_SIZE_MIN) ||
	    icm_data > MLXBF_LARGE_ICMC_SIZE_MAX || icm_data % MLXBF_LARGE_ICMC_GRANULARITY)
		return -EPERM;

	mutex_lock(&icm_ops_lock);
	arm_smccc_smc(MLNX_HANDLE_SET_ICM_INFO, icm_data, 0, 0, 0, 0, 0, 0, &res);
	mutex_unlock(&icm_ops_lock);

	return res.a0 ? -EPERM : count;
}

static ssize_t os_up_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct arm_smccc_res res;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val != 1)
		return -EINVAL;

	mutex_lock(&os_up_lock);
	arm_smccc_smc(MLNX_HANDLE_OS_UP, 0, 0, 0, 0, 0, 0, 0, &res);
	mutex_unlock(&os_up_lock);

	return count;
}

static ssize_t oob_mac_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct arm_smccc_res res;
	u8 *mac_byte_ptr;

	mutex_lock(&mfg_ops_lock);
	arm_smccc_smc(MLXBF_BOOTCTL_GET_MFG_INFO, MLNX_MFG_TYPE_OOB_MAC, 0, 0, 0,
		      0, 0, 0, &res);
	mutex_unlock(&mfg_ops_lock);
	if (res.a0)
		return -EPERM;

	mac_byte_ptr = (u8 *)&res.a1;

	return sysfs_format_mac(buf, mac_byte_ptr, ETH_ALEN);
}

static ssize_t oob_mac_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned int byte[MLNX_MFG_OOB_MAC_FORMAT_LEN] = { 0 };
	struct arm_smccc_res res;
	int byte_idx, len;
	u64 mac_addr = 0;
	u8 *mac_byte_ptr;

	if ((count - 1) != MLNX_MFG_OOB_MAC_FORMAT_LEN)
		return -EINVAL;

	len = sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		     &byte[0], &byte[1], &byte[2],
		     &byte[3], &byte[4], &byte[5]);
	if (len != ETH_ALEN)
		return -EINVAL;

	mac_byte_ptr = (u8 *)&mac_addr;

	for (byte_idx = 0; byte_idx < ETH_ALEN; byte_idx++)
		mac_byte_ptr[byte_idx] = (u8)byte[byte_idx];

	mutex_lock(&mfg_ops_lock);
	arm_smccc_smc(MLXBF_BOOTCTL_SET_MFG_INFO, MLNX_MFG_TYPE_OOB_MAC,
		      ETH_ALEN, mac_addr, 0, 0, 0, 0, &res);
	mutex_unlock(&mfg_ops_lock);

	return res.a0 ? -EPERM : count;
}

static ssize_t opn_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u64 opn_data[MLNX_MFG_VAL_QWORD_CNT(OPN) + 1] = { 0 };
	struct arm_smccc_res res;
	int word;

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(OPN); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_GET_MFG_INFO,
			      MLNX_MFG_TYPE_OPN_0 + word,
			      0, 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
		opn_data[word] = res.a1;
	}
	mutex_unlock(&mfg_ops_lock);

	return snprintf(buf, PAGE_SIZE, "%s", (char *)opn_data);
}

static ssize_t opn_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	u64 opn[MLNX_MFG_VAL_QWORD_CNT(OPN)] = { 0 };
	struct arm_smccc_res res;
	int word;

	if (count > MLNX_MFG_OPN_VAL_LEN)
		return -EINVAL;

	memcpy(opn, buf, count);

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(OPN); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_SET_MFG_INFO,
			      MLNX_MFG_TYPE_OPN_0 + word,
			      sizeof(u64), opn[word], 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
	}
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static ssize_t sku_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u64 sku_data[MLNX_MFG_VAL_QWORD_CNT(SKU) + 1] = { 0 };
	struct arm_smccc_res res;
	int word;

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(SKU); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_GET_MFG_INFO,
			      MLNX_MFG_TYPE_SKU_0 + word,
			      0, 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
		sku_data[word] = res.a1;
	}
	mutex_unlock(&mfg_ops_lock);

	return snprintf(buf, PAGE_SIZE, "%s", (char *)sku_data);
}

static ssize_t sku_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	u64 sku[MLNX_MFG_VAL_QWORD_CNT(SKU)] = { 0 };
	struct arm_smccc_res res;
	int word;

	if (count > MLNX_MFG_SKU_VAL_LEN)
		return -EINVAL;

	memcpy(sku, buf, count);

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(SKU); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_SET_MFG_INFO,
			      MLNX_MFG_TYPE_SKU_0 + word,
			      sizeof(u64), sku[word], 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
	}
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static ssize_t modl_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	u64 modl_data[MLNX_MFG_VAL_QWORD_CNT(MODL) + 1] = { 0 };
	struct arm_smccc_res res;
	int word;

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(MODL); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_GET_MFG_INFO,
			      MLNX_MFG_TYPE_MODL_0 + word,
			      0, 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
		modl_data[word] = res.a1;
	}
	mutex_unlock(&mfg_ops_lock);

	return snprintf(buf, PAGE_SIZE, "%s", (char *)modl_data);
}

static ssize_t modl_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u64 modl[MLNX_MFG_VAL_QWORD_CNT(MODL)] = { 0 };
	struct arm_smccc_res res;
	int word;

	if (count > MLNX_MFG_MODL_VAL_LEN)
		return -EINVAL;

	memcpy(modl, buf, count);

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(MODL); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_SET_MFG_INFO,
			      MLNX_MFG_TYPE_MODL_0 + word,
			      sizeof(u64), modl[word], 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
	}
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static ssize_t sn_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	u64 sn_data[MLNX_MFG_VAL_QWORD_CNT(SN) + 1] = { 0 };
	struct arm_smccc_res res;
	int word;

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(SN); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_GET_MFG_INFO,
			      MLNX_MFG_TYPE_SN_0 + word,
			      0, 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
		sn_data[word] = res.a1;
	}
	mutex_unlock(&mfg_ops_lock);

	return snprintf(buf, PAGE_SIZE, "%s", (char *)sn_data);
}

static ssize_t sn_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	u64 sn[MLNX_MFG_VAL_QWORD_CNT(SN)] = { 0 };
	struct arm_smccc_res res;
	int word;

	if (count > MLNX_MFG_SN_VAL_LEN)
		return -EINVAL;

	memcpy(sn, buf, count);

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(SN); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_SET_MFG_INFO,
			      MLNX_MFG_TYPE_SN_0 + word,
			      sizeof(u64), sn[word], 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
	}
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static ssize_t uuid_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	u64 uuid_data[MLNX_MFG_VAL_QWORD_CNT(UUID) + 1] = { 0 };
	struct arm_smccc_res res;
	int word;

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(UUID); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_GET_MFG_INFO,
			      MLNX_MFG_TYPE_UUID_0 + word,
			      0, 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
		uuid_data[word] = res.a1;
	}
	mutex_unlock(&mfg_ops_lock);

	return snprintf(buf, PAGE_SIZE, "%s", (char *)uuid_data);
}

static ssize_t uuid_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u64 uuid[MLNX_MFG_VAL_QWORD_CNT(UUID)] = { 0 };
	struct arm_smccc_res res;
	int word;

	if (count > MLNX_MFG_UUID_VAL_LEN)
		return -EINVAL;

	memcpy(uuid, buf, count);

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(UUID); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_SET_MFG_INFO,
			      MLNX_MFG_TYPE_UUID_0 + word,
			      sizeof(u64), uuid[word], 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
	}
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static ssize_t rev_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u64 rev_data[MLNX_MFG_VAL_QWORD_CNT(REV) + 1] = { 0 };
	struct arm_smccc_res res;
	int word;

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(REV); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_GET_MFG_INFO,
			      MLNX_MFG_TYPE_REV + word,
			      0, 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
		rev_data[word] = res.a1;
	}
	mutex_unlock(&mfg_ops_lock);

	return snprintf(buf, PAGE_SIZE, "%s", (char *)rev_data);
}

static ssize_t rev_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	u64 rev[MLNX_MFG_VAL_QWORD_CNT(REV)] = { 0 };
	struct arm_smccc_res res;
	int word;

	if (count > MLNX_MFG_REV_VAL_LEN)
		return -EINVAL;

	memcpy(rev, buf, count);

	mutex_lock(&mfg_ops_lock);
	for (word = 0; word < MLNX_MFG_VAL_QWORD_CNT(REV); word++) {
		arm_smccc_smc(MLXBF_BOOTCTL_SET_MFG_INFO,
			      MLNX_MFG_TYPE_REV + word,
			      sizeof(u64), rev[word], 0, 0, 0, 0, &res);
		if (res.a0) {
			mutex_unlock(&mfg_ops_lock);
			return -EPERM;
		}
	}
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static ssize_t mfg_lock_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct arm_smccc_res res;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val != 1)
		return -EINVAL;

	mutex_lock(&mfg_ops_lock);
	arm_smccc_smc(MLXBF_BOOTCTL_LOCK_MFG_INFO, 0, 0, 0, 0, 0, 0, 0, &res);
	mutex_unlock(&mfg_ops_lock);

	return count;
}

static char *rsh_log_get_reg_name(u64 opcode)
{
	struct rsh_log_reg *reg = rsh_log_regs;

	while (reg->name) {
		if (reg->opcode == opcode)
			return reg->name;
		reg++;
	}

	return "unknown";
}

static int rsh_log_show_crash(u64 hdr, char *buf, int size)
{
	int i, module, type, len, n = 0;
	u32 pc, syndrome, ec;
	u64 opcode, data;
	char *p = buf;

	module = BF_RSH_LOG_HEADER_GET(MOD, hdr);
	if (module >= ARRAY_SIZE(mlxbf_rsh_log_mod))
		module = 0;
	type = BF_RSH_LOG_HEADER_GET(TYPE, hdr);
	len = BF_RSH_LOG_HEADER_GET(LEN, hdr);

	if (type == BF_RSH_LOG_TYPE_EXCEPTION) {
		syndrome = BF_RSH_LOG_HEADER_GET(SYNDROME, hdr);
		ec = syndrome >> AARCH64_ESR_ELX_EXCEPTION_CLASS_SHIFT;
		n = snprintf(p, size, " Exception(%s): syndrome = 0x%x%s\n",
			    mlxbf_rsh_log_mod[module], syndrome,
			    (ec == 0x24 || ec == 0x25) ? "(Data Abort)" :
			    (ec == 0x2f) ? "(SError)" : "");
	} else if (type == BF_RSH_LOG_TYPE_PANIC) {
		pc = BF_RSH_LOG_HEADER_GET(PC, hdr);
		n = snprintf(p, size,
			     " PANIC(%s): PC = 0x%x\n", mlxbf_rsh_log_mod[module],
			     pc);
	}
	if (n > 0) {
		p += n;
		size -= n;
	}

	/*
	 * Read the registers in a loop. 'len' is the total number of words in
	 * 8-bytes. Two words are read in each loop.
	 */
	for (i = 0; i < len/2; i++) {
		opcode = readq(mlxbf_rsh_scratch_buf_data);
		data = readq(mlxbf_rsh_scratch_buf_data);

		opcode = (opcode >> AARCH64_MRS_REG_SHIFT) &
			AARCH64_MRS_REG_MASK;
		n = snprintf(p, size,
			     "   %-16s0x%llx\n", rsh_log_get_reg_name(opcode),
			     (unsigned long long)data);
		if (n > 0) {
			p += n;
			size -= n;
		}
	}

	return p - buf;
}

static int rsh_log_format_msg(char *buf, int size, const char *msg, ...)
{
	va_list args;
	int len;

	va_start(args, msg);
	len = vsnprintf(buf, size, msg, args);
	va_end(args);

	return len;
}

static int rsh_log_show_msg(u64 hdr, char *buf, int size)
{
	int has_arg = BF_RSH_LOG_HEADER_GET(HAS_ARG, hdr);
	int level = BF_RSH_LOG_HEADER_GET(LEVEL, hdr);
	int module = BF_RSH_LOG_HEADER_GET(MOD, hdr);
	int len = BF_RSH_LOG_HEADER_GET(LEN, hdr);
	u32 arg = BF_RSH_LOG_HEADER_GET(ARG, hdr);
	char *msg, *p;
	u64 data;

	if (len <= 0)
		return -EINVAL;

	if (module >= ARRAY_SIZE(mlxbf_rsh_log_mod))
		module = 0;

	if (level >= ARRAY_SIZE(mlxbf_rsh_log_level))
		level = 0;

	msg = kmalloc(len * sizeof(u64) + 1, GFP_KERNEL);
	if (!msg)
		return 0;
	p = msg;

	while (len--) {
		data = readq(mlxbf_rsh_scratch_buf_data);
		memcpy(p, &data, sizeof(data));
		p += sizeof(data);
	}
	*p = '\0';
	if (!has_arg) {
		len = snprintf(buf, size, " %s[%s]: %s\n", mlxbf_rsh_log_level[level],
			       mlxbf_rsh_log_mod[module], msg);
	} else {
		len = snprintf(buf, size, " %s[%s]: ", mlxbf_rsh_log_level[level],
			       mlxbf_rsh_log_mod[module]);
		len += rsh_log_format_msg(buf + len, size - len, msg, arg);
		len += snprintf(buf + len, size - len, "\n");
	}

	kfree(msg);
	return len;
}

static ssize_t rsh_log_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	u64 hdr;
	char *p = buf;
	int i, n, rc, idx, type, len, size = PAGE_SIZE;

	if (!mlxbf_rsh_semaphore || !mlxbf_rsh_scratch_buf_ctl)
		return -EOPNOTSUPP;

	/* Take the semaphore. */
	rc = mlxbf_rsh_log_sem_lock();
	if (rc)
		return rc;

	/* Save the current index and read from 0. */
	idx = readq(mlxbf_rsh_scratch_buf_ctl) & RSH_SCRATCH_BUF_CTL_IDX_MASK;
	if (!idx)
		goto done;
	writeq(0, mlxbf_rsh_scratch_buf_ctl);

	i = 0;
	while (i < idx) {
		hdr = readq(mlxbf_rsh_scratch_buf_data);
		type = BF_RSH_LOG_HEADER_GET(TYPE, hdr);
		len = BF_RSH_LOG_HEADER_GET(LEN, hdr);
		i += 1 + len;
		if (i > idx)
			break;

		switch (type) {
		case BF_RSH_LOG_TYPE_PANIC:
		case BF_RSH_LOG_TYPE_EXCEPTION:
			n = rsh_log_show_crash(hdr, p, size);
			p += n;
			size -= n;
			break;
		case BF_RSH_LOG_TYPE_MSG:
			n = rsh_log_show_msg(hdr, p, size);
			p += n;
			size -= n;
			break;
		default:
			/* Drain this message. */
			while (len--)
				(void) readq(mlxbf_rsh_scratch_buf_data);
			break;
		}
	}

	if (rsh_log_clear_on_read)
		writeq(0, mlxbf_rsh_scratch_buf_ctl);
	else
		writeq(idx, mlxbf_rsh_scratch_buf_ctl);

done:
	/* Release the semaphore. */
	mlxbf_rsh_log_sem_unlock();

	return p - buf;
}

static DEVICE_ATTR_RW(post_reset_wdog);
static DEVICE_ATTR_RW(reset_action);
static DEVICE_ATTR_RW(second_reset_action);
static DEVICE_ATTR_RO(lifecycle_state);
static DEVICE_ATTR_RO(secure_boot_fuse_state);
static DEVICE_ATTR_WO(fw_reset);
static DEVICE_ATTR_RW(rsh_log);
static DEVICE_ATTR_RW(large_icm);
static DEVICE_ATTR_WO(os_up);
static DEVICE_ATTR_RW(oob_mac);
static DEVICE_ATTR_RW(opn);
static DEVICE_ATTR_RW(sku);
static DEVICE_ATTR_RW(modl);
static DEVICE_ATTR_RW(sn);
static DEVICE_ATTR_RW(uuid);
static DEVICE_ATTR_RW(rev);
static DEVICE_ATTR_WO(mfg_lock);

static struct attribute *mlxbf_bootctl_attrs[] = {
	&dev_attr_post_reset_wdog.attr,
	&dev_attr_reset_action.attr,
	&dev_attr_second_reset_action.attr,
	&dev_attr_lifecycle_state.attr,
	&dev_attr_secure_boot_fuse_state.attr,
	&dev_attr_fw_reset.attr,
	&dev_attr_rsh_log.attr,
	&dev_attr_large_icm.attr,
	&dev_attr_os_up.attr,
	&dev_attr_oob_mac.attr,
	&dev_attr_opn.attr,
	&dev_attr_sku.attr,
	&dev_attr_modl.attr,
	&dev_attr_sn.attr,
	&dev_attr_uuid.attr,
	&dev_attr_rev.attr,
	&dev_attr_mfg_lock.attr,
	NULL
};

ATTRIBUTE_GROUPS(mlxbf_bootctl);

static const struct acpi_device_id mlxbf_bootctl_acpi_ids[] = {
	{"MLNXBF04", 0},
	{}
};

MODULE_DEVICE_TABLE(acpi, mlxbf_bootctl_acpi_ids);

static ssize_t mlxbf_bootctl_bootfifo_read(struct file *filp,
					   struct kobject *kobj,
					   struct bin_attribute *bin_attr,
					   char *buf, loff_t pos,
					   size_t count)
{
	unsigned long timeout = msecs_to_jiffies(500);
	unsigned long expire = jiffies + timeout;
	u64 data, cnt = 0;
	char *p = buf;

	while (count >= sizeof(data)) {
		/* Give up reading if no more data within 500ms. */
		if (!cnt) {
			cnt = readq(mlxbf_rsh_boot_cnt);
			if (!cnt) {
				if (time_after(jiffies, expire))
					break;
				usleep_range(10, 50);
				continue;
			}
		}

		data = readq(mlxbf_rsh_boot_data);
		memcpy(p, &data, sizeof(data));
		count -= sizeof(data);
		p += sizeof(data);
		cnt--;
		expire = jiffies + timeout;
	}

	return p - buf;
}

static struct bin_attribute mlxbf_bootctl_bootfifo_sysfs_attr = {
	.attr = { .name = "bootfifo", .mode = 0400 },
	.read = mlxbf_bootctl_bootfifo_read,
};

static bool mlxbf_bootctl_guid_match(const guid_t *guid,
				     const struct arm_smccc_res *res)
{
	guid_t id = GUID_INIT(res->a0, res->a1, res->a1 >> 16,
			      res->a2, res->a2 >> 8, res->a2 >> 16,
			      res->a2 >> 24, res->a3, res->a3 >> 8,
			      res->a3 >> 16, res->a3 >> 24);

	return guid_equal(guid, &id);
}

static int mlxbf_bootctl_probe(struct platform_device *pdev)
{
	struct arm_smccc_res res = { 0 };
	void __iomem *reg;
	guid_t guid;
	int ret;

	/* Map the resource of the bootfifo data register. */
	mlxbf_rsh_boot_data = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mlxbf_rsh_boot_data))
		return PTR_ERR(mlxbf_rsh_boot_data);

	/* Map the resource of the bootfifo counter register. */
	mlxbf_rsh_boot_cnt = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(mlxbf_rsh_boot_cnt))
		return PTR_ERR(mlxbf_rsh_boot_cnt);

	/* Map the resource of the rshim semaphore register. */
	mlxbf_rsh_semaphore = devm_platform_ioremap_resource(pdev, 2);
	if (IS_ERR(mlxbf_rsh_semaphore))
		return PTR_ERR(mlxbf_rsh_semaphore);

	/* Map the resource of the scratch buffer (log) registers. */
	reg = devm_platform_ioremap_resource(pdev, 3);
	if (IS_ERR(reg))
		return PTR_ERR(reg);
	mlxbf_rsh_scratch_buf_ctl = reg + MLXBF_RSH_SCRATCH_BUF_CTL_OFF;
	mlxbf_rsh_scratch_buf_data = reg + MLXBF_RSH_SCRATCH_BUF_DATA_OFF;

	/* Ensure we have the UUID we expect for this service. */
	arm_smccc_smc(MLXBF_BOOTCTL_SIP_SVC_UID, 0, 0, 0, 0, 0, 0, 0, &res);
	guid_parse(mlxbf_bootctl_svc_uuid_str, &guid);
	if (!mlxbf_bootctl_guid_match(&guid, &res))
		return -ENODEV;

	/*
	 * When watchdog is used, it sets boot mode to MLXBF_BOOTCTL_SWAP_EMMC
	 * in case of boot failures. However it doesn't clear the state if there
	 * is no failure. Restore the default boot mode here to avoid any
	 * unnecessary boot partition swapping.
	 */
	ret = mlxbf_bootctl_smc(MLXBF_BOOTCTL_SET_RESET_ACTION,
				MLXBF_BOOTCTL_EMMC);
	if (ret < 0)
		dev_warn(&pdev->dev, "Unable to reset the EMMC boot mode\n");

	ret = sysfs_create_bin_file(&pdev->dev.kobj,
				    &mlxbf_bootctl_bootfifo_sysfs_attr);
	if (ret)
		pr_err("Unable to create bootfifo sysfs file, error %d\n", ret);

	return ret;
}

static int mlxbf_bootctl_remove(struct platform_device *pdev)
{
	sysfs_remove_bin_file(&pdev->dev.kobj,
			      &mlxbf_bootctl_bootfifo_sysfs_attr);

	return 0;
}

static struct platform_driver mlxbf_bootctl_driver = {
	.probe = mlxbf_bootctl_probe,
	.remove = mlxbf_bootctl_remove,
	.driver = {
		.name = "mlxbf-bootctl",
		.dev_groups = mlxbf_bootctl_groups,
		.acpi_match_table = mlxbf_bootctl_acpi_ids,
	}
};

module_platform_driver(mlxbf_bootctl_driver);

MODULE_DESCRIPTION("Mellanox boot control driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Mellanox Technologies");
