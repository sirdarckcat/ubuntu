// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Broadcom BCM2712 GPIO units (pinctrl only)
 *
 * Copyright (C) 2021 Raspberry Pi (Trading) Ltd.
 * Copyright (C) 2012 Chris Boot, Simon Arlott, Stephen Warren
 *
 * Based heavily on the BCM2835 GPIO & pinctrl driver, which was inspired by:
 * pinctrl-nomadik.c, please see original file for copyright information
 * pinctrl-nomadik.c, please see original file for copyright information
 * pinctrl-tegra.c, please see original file for copyright information
 */

#include <linux/bitmap.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <dt-bindings/pinctrl/bcm2712.h>

#define MODULE_NAME "pinctrl-bcm2712"
#define BCM2712_NUM_GPIOS 55
#define BCM2712_NUM_AON_GPIOS 22
#define BCM7712_NUM_GPIOS 48
#define BCM7712_NUM_AON_GPIOS 22

/* Register offsets */
#define GPIO_PINMUX_0		0x00
#define GPIO_PADCTRL_0		0x18

#define AON_GPIO_PINMUX_0	0x00
#define AON_GPIO_PINMUX_3	0x0c
#define AON_GPIO_PADCTRL_0	0x18

#define BCM2712_PULL_MASK	0x3

/* argument: bcm2712_pinconf_pull */
#define BCM2712_PINCONF_PARAM_PULL	(PIN_CONFIG_END + 1)

struct pin_regs {
	u16 mux_bit;
	u16 pad_bit;
};

struct bcm2712_pinctrl {
	struct device *dev;
	void __iomem *base;
	struct pinctrl_dev *pctl_dev;
	struct pinctrl_desc pctl_desc;
	const struct pin_regs *pin_regs;
	const char *const *gpio_groups;
	struct pinctrl_gpio_range gpio_range;
	spinlock_t lock;
};

struct bcm_plat_data {
	const struct pinctrl_desc *pctl_desc;
	const struct pinctrl_gpio_range *gpio_range;
	const struct pin_regs *pin_regs;
};

#define REG_BIT_INVALID 0xffff

#define BIT_TO_REG(b) (((b) >> 5) << 2)
#define BIT_TO_SHIFT(b) ((b) & 0x1f)

#define GPIO_REGS(n,r,b) \
	{ GPIO_PINMUX_0*8 + (n)*4, (GPIO_PADCTRL_0 + (r)*4)*8 + (b)*2 }

#define EMMC_REGS(r,b) \
	{ 0, (GPIO_PADCTRL_0 + (r)*4)*8 + (b)*2 }

#define AGPIO_REGS(n,pr,pb) \
	{ AON_GPIO_PINMUX_3*8 + (n)*4, (AON_GPIO_PADCTRL_0 + (pr)*4)*8 + (pb)*2 }

#define SGPIO_REGS(mr,mb) \
	{ (AON_GPIO_PINMUX_0 + (mr)*4)*8 + (mb)*4, REG_BIT_INVALID }

static const struct pin_regs bcm2712_gpio_pin_regs[] = {
	GPIO_REGS(0, 0, 7),
	GPIO_REGS(1, 0, 8),
	GPIO_REGS(2, 0, 9),
	GPIO_REGS(3, 0, 10),
	GPIO_REGS(4, 0, 11),
	GPIO_REGS(5, 0, 12),
	GPIO_REGS(6, 0, 13),
	GPIO_REGS(7, 0, 14),
	GPIO_REGS(8, 1, 0),
	GPIO_REGS(9, 1, 1),
	GPIO_REGS(10, 1, 2),
	GPIO_REGS(11, 1, 3),
	GPIO_REGS(12, 1, 4),
	GPIO_REGS(13, 1, 5),
	GPIO_REGS(14, 1, 6),
	GPIO_REGS(15, 1, 7),
	GPIO_REGS(16, 1, 8),
	GPIO_REGS(17, 1, 9),
	GPIO_REGS(18, 1, 10),
	GPIO_REGS(19, 1, 11),
	GPIO_REGS(20, 1, 12),
	GPIO_REGS(21, 1, 13),
	GPIO_REGS(22, 1, 14),
	GPIO_REGS(23, 2, 0),
	GPIO_REGS(24, 2, 1),
	GPIO_REGS(25, 2, 2),
	GPIO_REGS(26, 2, 3),
	GPIO_REGS(27, 2, 4),
	GPIO_REGS(28, 2, 5),
	GPIO_REGS(29, 2, 6),
	GPIO_REGS(30, 2, 7),
	GPIO_REGS(31, 2, 8),
	GPIO_REGS(32, 2, 9),
	GPIO_REGS(33, 2, 10),
	GPIO_REGS(34, 2, 11),
	GPIO_REGS(35, 2, 12),
	GPIO_REGS(36, 2, 13),
	GPIO_REGS(37, 2, 14),
	GPIO_REGS(38, 3, 0),
	GPIO_REGS(39, 3, 1),
	GPIO_REGS(40, 3, 2),
	GPIO_REGS(41, 3, 3),
	GPIO_REGS(42, 3, 4),
	GPIO_REGS(43, 3, 5),
	GPIO_REGS(44, 3, 6),
	GPIO_REGS(45, 3, 7),
	GPIO_REGS(46, 3, 8),
	GPIO_REGS(47, 3, 9),
	/* "more" registers for EMMC pad controls - no mux sel */
	EMMC_REGS(3, 10), /* EMMC_CMD */
	EMMC_REGS(3, 11), /* EMMC_DS */
	EMMC_REGS(3, 12), /* EMMC_CLK */
	EMMC_REGS(3, 13), /* EMMC_DAT0 */
	EMMC_REGS(3, 14), /* EMMC_DAT1 */
	EMMC_REGS(4, 0),  /* EMMC_DAT2 */
	EMMC_REGS(4, 1),  /* EMMC_DAT3 */
};

static struct pin_regs bcm2712_aon_gpio_pin_regs[] = {
	AGPIO_REGS(0, 0, 10),
	AGPIO_REGS(1, 0, 11),
	AGPIO_REGS(2, 0, 12),
	AGPIO_REGS(3, 0, 13),
	AGPIO_REGS(4, 0, 14),
	AGPIO_REGS(5, 1, 0),
	AGPIO_REGS(6, 1, 1),
	AGPIO_REGS(7, 1, 2),
	AGPIO_REGS(8, 1, 3),
	AGPIO_REGS(9, 1, 4),
	AGPIO_REGS(10, 1, 5),
	AGPIO_REGS(11, 1, 6),
	AGPIO_REGS(12, 1, 7),
	AGPIO_REGS(13, 1, 8),
	AGPIO_REGS(14, 1, 9),
	AGPIO_REGS(15, 1, 10),
	SGPIO_REGS(0, 0),
	SGPIO_REGS(0, 1),
	SGPIO_REGS(0, 2),
	SGPIO_REGS(0, 3),
	SGPIO_REGS(1, 0),
	SGPIO_REGS(2, 0),
};

#define GPIO_PIN(a) PINCTRL_PIN(a, "gpio" #a)
#define AGPIO_PIN(a) PINCTRL_PIN(a, "aon_gpio" #a)
#define SGPIO_PIN(a) PINCTRL_PIN(a+16, "aon_sgpio" #a)

static const struct pinctrl_pin_desc bcm2712_gpio_pins[] = {
	GPIO_PIN(0),
	GPIO_PIN(1),
	GPIO_PIN(2),
	GPIO_PIN(3),
	GPIO_PIN(4),
	GPIO_PIN(5),
	GPIO_PIN(6),
	GPIO_PIN(7),
	GPIO_PIN(8),
	GPIO_PIN(9),
	GPIO_PIN(10),
	GPIO_PIN(11),
	GPIO_PIN(12),
	GPIO_PIN(13),
	GPIO_PIN(14),
	GPIO_PIN(15),
	GPIO_PIN(16),
	GPIO_PIN(17),
	GPIO_PIN(18),
	GPIO_PIN(19),
	GPIO_PIN(20),
	GPIO_PIN(21),
	GPIO_PIN(22),
	GPIO_PIN(23),
	GPIO_PIN(24),
	GPIO_PIN(25),
	GPIO_PIN(26),
	GPIO_PIN(27),
	GPIO_PIN(28),
	GPIO_PIN(29),
	GPIO_PIN(30),
	GPIO_PIN(31),
	GPIO_PIN(32),
	GPIO_PIN(33),
	GPIO_PIN(34),
	GPIO_PIN(35),
	GPIO_PIN(36),
	GPIO_PIN(37),
	GPIO_PIN(38),
	GPIO_PIN(39),
	GPIO_PIN(40),
	GPIO_PIN(41),
	GPIO_PIN(42),
	GPIO_PIN(43),
	GPIO_PIN(44),
	GPIO_PIN(45),
	GPIO_PIN(46),
	GPIO_PIN(47),
	PINCTRL_PIN(48, "emmc_cmd"),
	PINCTRL_PIN(49, "emmc_ds"),
	PINCTRL_PIN(50, "emmc_clk"),
	PINCTRL_PIN(51, "emmc_dat0"),
	PINCTRL_PIN(52, "emmc_dat1"),
	PINCTRL_PIN(53, "emmc_dat2"),
	PINCTRL_PIN(54, "emmc_dat3"),
};

static struct pinctrl_pin_desc bcm2712_aon_gpio_pins[] = {
	AGPIO_PIN(0),
	AGPIO_PIN(1),
	AGPIO_PIN(2),
	AGPIO_PIN(3),
	AGPIO_PIN(4),
	AGPIO_PIN(5),
	AGPIO_PIN(6),
	AGPIO_PIN(7),
	AGPIO_PIN(8),
	AGPIO_PIN(9),
	AGPIO_PIN(10),
	AGPIO_PIN(11),
	AGPIO_PIN(12),
	AGPIO_PIN(13),
	AGPIO_PIN(14),
	AGPIO_PIN(15),
	SGPIO_PIN(0),
	SGPIO_PIN(1),
	SGPIO_PIN(2),
	SGPIO_PIN(3),
	SGPIO_PIN(4),
	SGPIO_PIN(5),
};

enum bcm2712_fsel {
	BCM2712_FSEL_COUNT = 10,
	BCM2712_FSEL_MASK = 0xf,
};

static const char * const bcm2712_functions[BCM2712_FSEL_COUNT] = {
	[BCM2712_FSEL_GPIO] = "gpio",
	[BCM2712_FSEL_ALT1] = "alt1",
	[BCM2712_FSEL_ALT2] = "alt2",
	[BCM2712_FSEL_ALT3] = "alt3",
	[BCM2712_FSEL_ALT4] = "alt4",
	[BCM2712_FSEL_ALT5] = "alt5",
	[BCM2712_FSEL_ALT6] = "alt6",
	[BCM2712_FSEL_ALT7] = "alt7",
	[BCM2712_FSEL_ALT8] = "alt8",
	[BCM2712_FSEL_ALT9] = "alt9",
};

static inline u32 bcm2712_reg_rd(struct bcm2712_pinctrl *pc, unsigned reg)
{
	return readl(pc->base + reg);
}

static inline void bcm2712_reg_wr(struct bcm2712_pinctrl *pc, unsigned reg,
		u32 val)
{
	writel(val, pc->base + reg);
}

static inline enum bcm2712_fsel bcm2712_pinctrl_fsel_get(
		struct bcm2712_pinctrl *pc, unsigned pin)
{
	u32 bit = pc->pin_regs[pin].mux_bit;
	u32 val;
	enum bcm2712_fsel status;
	if (!bit)
		return BCM2712_FSEL_GPIO;

	val = bcm2712_reg_rd(pc, BIT_TO_REG(bit));
	status = (val >> BIT_TO_SHIFT(bit)) & BCM2712_FSEL_MASK;

	dev_dbg(pc->dev, "get %08x (%u => %s)\n", val, pin,
			bcm2712_functions[status]);

	return status;
}

static inline void bcm2712_pinctrl_fsel_set(
		struct bcm2712_pinctrl *pc, unsigned pin,
		enum bcm2712_fsel fsel)
{
	u32 bit = pc->pin_regs[pin].mux_bit, val;
	enum bcm2712_fsel cur;
	unsigned long flags;

	if (!bit)
		return;

	spin_lock_irqsave(&pc->lock, flags);

	val = bcm2712_reg_rd(pc, BIT_TO_REG(bit));
	cur = (val >> BIT_TO_SHIFT(bit)) & BCM2712_FSEL_MASK;

	dev_dbg(pc->dev, "read %08x (%u => %s)\n", val, pin,
			bcm2712_functions[cur]);

	if (cur != fsel) {
		val &= ~(BCM2712_FSEL_MASK << BIT_TO_SHIFT(bit));
		val |= fsel << BIT_TO_SHIFT(bit);

		dev_dbg(pc->dev, "write %08x (%u <= %s)\n", val, pin,
			bcm2712_functions[fsel]);
		bcm2712_reg_wr(pc, BIT_TO_REG(bit), val);
	}

	spin_unlock_irqrestore(&pc->lock, flags);
}

static int bcm2712_pctl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	return pc->pctl_desc.npins;
}

static const char *bcm2712_pctl_get_group_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	return pc->gpio_groups[selector];
}

static int bcm2712_pctl_get_group_pins(struct pinctrl_dev *pctldev,
		unsigned selector,
		const unsigned **pins,
		unsigned *num_pins)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	*pins = &pc->pctl_desc.pins[selector].number;
	*num_pins = 1;

	return 0;
}

static void bcm2712_pctl_pin_dbg_show(struct pinctrl_dev *pctldev,
		struct seq_file *s,
		unsigned offset)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	enum bcm2712_fsel fsel = bcm2712_pinctrl_fsel_get(pc, offset);
	const char *fname = bcm2712_functions[fsel];

	seq_printf(s, "function %s", fname);
}

static void bcm2712_pctl_dt_free_map(struct pinctrl_dev *pctldev,
		struct pinctrl_map *maps, unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++)
		if (maps[i].type == PIN_MAP_TYPE_CONFIGS_PIN)
			kfree(maps[i].data.configs.configs);

	kfree(maps);
}

static int bcm2712_pctl_dt_node_to_map_func(struct bcm2712_pinctrl *pc,
		struct device_node *np, u32 pin, u32 fnum,
		struct pinctrl_map **maps)
{
	struct pinctrl_map *map = *maps;

	if (fnum >= ARRAY_SIZE(bcm2712_functions)) {
		dev_err(pc->dev, "%pOF: invalid brcm,function %d\n", np, fnum);
		return -EINVAL;
	}

	map->type = PIN_MAP_TYPE_MUX_GROUP;
	map->data.mux.group = pc->gpio_groups[pin];
	map->data.mux.function = bcm2712_functions[fnum];
	(*maps)++;

	return 0;
}

static int bcm2712_pctl_dt_node_to_map_pull(struct bcm2712_pinctrl *pc,
		struct device_node *np, u32 pin, u32 pull,
		struct pinctrl_map **maps)
{
	struct pinctrl_map *map = *maps;
	unsigned long *configs;

	if (pull > 2) {
		dev_err(pc->dev, "%pOF: invalid brcm,pull %d\n", np, pull);
		return -EINVAL;
	}

	configs = kzalloc(sizeof(*configs), GFP_KERNEL);
	if (!configs)
		return -ENOMEM;
	configs[0] = pinconf_to_config_packed(BCM2712_PINCONF_PARAM_PULL, pull);

	map->type = PIN_MAP_TYPE_CONFIGS_PIN;
	map->data.configs.group_or_pin = pc->pctl_desc.pins[pin].name;
	map->data.configs.configs = configs;
	map->data.configs.num_configs = 1;
	(*maps)++;

	return 0;
}

static int bcm2712_pctl_dt_node_to_map(struct pinctrl_dev *pctldev,
		struct device_node *np,
		struct pinctrl_map **map, unsigned int *num_maps)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	struct property *pins, *funcs, *pulls;
	int num_pins, num_funcs, num_pulls, maps_per_pin;
	struct pinctrl_map *maps, *cur_map;
	int i, err;
	u32 pin, func, pull;

	/* Check for generic binding in this node */
	err = pinconf_generic_dt_node_to_map_all(pctldev, np, map, num_maps);
	if (err || *num_maps)
		return err;

	/* Generic binding did not find anything continue with legacy parse */
	pins = of_find_property(np, "brcm,pins", NULL);
	if (!pins) {
		dev_err(pc->dev, "%pOF: missing brcm,pins property\n", np);
		return -EINVAL;
	}

	funcs = of_find_property(np, "brcm,function", NULL);
	pulls = of_find_property(np, "brcm,pull", NULL);

	if (!funcs && !pulls) {
		dev_err(pc->dev,
			"%pOF: neither brcm,function nor brcm,pull specified\n",
			np);
		return -EINVAL;
	}

	num_pins = pins->length / 4;
	num_funcs = funcs ? (funcs->length / 4) : 0;
	num_pulls = pulls ? (pulls->length / 4) : 0;

	if (num_funcs > 1 && num_funcs != num_pins) {
		dev_err(pc->dev,
			"%pOF: brcm,function must have 1 or %d entries\n",
			np, num_pins);
		return -EINVAL;
	}

	if (num_pulls > 1 && num_pulls != num_pins) {
		dev_err(pc->dev,
			"%pOF: brcm,pull must have 1 or %d entries\n",
			np, num_pins);
		return -EINVAL;
	}

	maps_per_pin = 0;
	if (num_funcs)
		maps_per_pin++;
	if (num_pulls)
		maps_per_pin++;
	cur_map = maps = kcalloc(num_pins * maps_per_pin, sizeof(*maps),
				 GFP_KERNEL);
	if (!maps)
		return -ENOMEM;

	for (i = 0; i < num_pins; i++) {
		err = of_property_read_u32_index(np, "brcm,pins", i, &pin);
		if (err)
			goto out;
		if (pin >= pc->pctl_desc.npins) {
			dev_err(pc->dev, "%pOF: invalid brcm,pins value %d\n",
				np, pin);
			err = -EINVAL;
			goto out;
		}

		if (num_funcs) {
			err = of_property_read_u32_index(np, "brcm,function",
					(num_funcs > 1) ? i : 0, &func);
			if (err)
				goto out;
			err = bcm2712_pctl_dt_node_to_map_func(pc, np, pin,
							func, &cur_map);
			if (err)
				goto out;
		}
		if (num_pulls) {
			err = of_property_read_u32_index(np, "brcm,pull",
					(num_pulls > 1) ? i : 0, &pull);
			if (err)
				goto out;
			err = bcm2712_pctl_dt_node_to_map_pull(pc, np, pin,
							pull, &cur_map);
			if (err)
				goto out;
		}
	}

	*map = maps;
	*num_maps = num_pins * maps_per_pin;

	return 0;

out:
	bcm2712_pctl_dt_free_map(pctldev, maps, num_pins * maps_per_pin);
	return err;
}

static const struct pinctrl_ops bcm2712_pctl_ops = {
	.get_groups_count = bcm2712_pctl_get_groups_count,
	.get_group_name = bcm2712_pctl_get_group_name,
	.get_group_pins = bcm2712_pctl_get_group_pins,
	.pin_dbg_show = bcm2712_pctl_pin_dbg_show,
	.dt_node_to_map = bcm2712_pctl_dt_node_to_map,
	.dt_free_map = bcm2712_pctl_dt_free_map,
};

static int bcm2712_pmx_free(struct pinctrl_dev *pctldev,
		unsigned offset)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	/* disable by setting to GPIO */
	bcm2712_pinctrl_fsel_set(pc, offset, BCM2712_FSEL_GPIO);
	return 0;
}

static int bcm2712_pmx_get_functions_count(struct pinctrl_dev *pctldev)
{
	return BCM2712_FSEL_COUNT;
}

static const char *bcm2712_pmx_get_function_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	return bcm2712_functions[selector];
}

static int bcm2712_pmx_get_function_groups(struct pinctrl_dev *pctldev,
		unsigned selector,
		const char * const **groups,
		unsigned * const num_groups)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	/* every pin can do every function */
	*groups = pc->gpio_groups;
	*num_groups = pc->pctl_desc.npins;

	return 0;
}

static int bcm2712_pmx_set(struct pinctrl_dev *pctldev,
		unsigned func_selector,
		unsigned group_selector)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	bcm2712_pinctrl_fsel_set(pc, group_selector, func_selector);

	return 0;
}
static int bcm2712_pmx_gpio_request_enable(struct pinctrl_dev *pctldev,
					   struct pinctrl_gpio_range *range,
					   unsigned pin)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	bcm2712_pinctrl_fsel_set(pc, pin, BCM2712_FSEL_GPIO);

	return 0;
}

static void bcm2712_pmx_gpio_disable_free(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range,
		unsigned offset)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	/* disable by setting to GPIO */
	bcm2712_pinctrl_fsel_set(pc, offset, BCM2712_FSEL_GPIO);
}

static const struct pinmux_ops bcm2712_pmx_ops = {
	.free = bcm2712_pmx_free,
	.get_functions_count = bcm2712_pmx_get_functions_count,
	.get_function_name = bcm2712_pmx_get_function_name,
	.get_function_groups = bcm2712_pmx_get_function_groups,
	.set_mux = bcm2712_pmx_set,
	.gpio_request_enable = bcm2712_pmx_gpio_request_enable,
	.gpio_disable_free = bcm2712_pmx_gpio_disable_free,
};

static int bcm2712_pinconf_get(struct pinctrl_dev *pctldev,
			unsigned pin, unsigned long *config)
{
	/* No way to read back config in HW */
	// FIXME
	return -ENOTSUPP;
}

static unsigned int bcm2712_pull_config_get(struct bcm2712_pinctrl *pc,
					    unsigned int pin)
{
	u32 bit = pc->pin_regs[pin].pad_bit, val;

	if (unlikely(bit == REG_BIT_INVALID))
	    return BCM2712_PULL_NONE;

	val = bcm2712_reg_rd(pc, BIT_TO_REG(bit));
	return (val >> BIT_TO_SHIFT(bit)) & BCM2712_PULL_MASK;
}

static void bcm2712_pull_config_set(struct bcm2712_pinctrl *pc,
				    unsigned int pin, unsigned int arg)
{
	u32 bit = pc->pin_regs[pin].pad_bit, val;
	unsigned long flags;

	if (unlikely(bit == REG_BIT_INVALID)) {
	    dev_warn(pc->dev, "can't set pulls for %s\n", pc->gpio_groups[pin]);
	    return;
	}

	spin_lock_irqsave(&pc->lock, flags);

	val = bcm2712_reg_rd(pc, BIT_TO_REG(bit));
	val &= ~(BCM2712_PULL_MASK << BIT_TO_SHIFT(bit));
	val |= (arg << BIT_TO_SHIFT(bit));
	bcm2712_reg_wr(pc, BIT_TO_REG(bit), val);

	spin_unlock_irqrestore(&pc->lock, flags);
}

static int bcm2712_pinconf_set(struct pinctrl_dev *pctldev,
			       unsigned int pin, unsigned long *configs,
			       unsigned int num_configs)
{
	struct bcm2712_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	u32 param, arg;
	int i;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case BCM2712_PINCONF_PARAM_PULL:
			bcm2712_pull_config_set(pc, pin, arg);
			break;

		/* Set pull generic bindings */
		case PIN_CONFIG_BIAS_DISABLE:
			bcm2712_pull_config_set(pc, pin, BCM2712_PULL_NONE);
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			bcm2712_pull_config_set(pc, pin, BCM2712_PULL_DOWN);
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			bcm2712_pull_config_set(pc, pin, BCM2712_PULL_UP);
			break;

		default:
			return -ENOTSUPP;
		}
	} /* for each config */

	return 0;
}

static const struct pinconf_ops bcm2712_pinconf_ops = {
	.is_generic = true,
	.pin_config_get = bcm2712_pinconf_get,
	.pin_config_set = bcm2712_pinconf_set,
};

static const struct pinctrl_desc bcm2712_pinctrl_desc = {
	.name = "pinctrl-bcm2712",
	.pins = bcm2712_gpio_pins,
	.npins = BCM2712_NUM_GPIOS,
	.pctlops = &bcm2712_pctl_ops,
	.pmxops = &bcm2712_pmx_ops,
	.confops = &bcm2712_pinconf_ops,
	.owner = THIS_MODULE,
};

static const struct pinctrl_desc bcm2712_aon_pinctrl_desc = {
	.name = "aon-pinctrl-bcm2712",
	.pins = bcm2712_aon_gpio_pins,
	.npins = BCM2712_NUM_AON_GPIOS,
	.pctlops = &bcm2712_pctl_ops,
	.pmxops = &bcm2712_pmx_ops,
	.confops = &bcm2712_pinconf_ops,
	.owner = THIS_MODULE,
};

static const struct pinctrl_gpio_range bcm2712_pinctrl_gpio_range = {
	.name = "pinctrl-bcm2712",
	.npins = BCM2712_NUM_GPIOS,
};

static const struct pinctrl_gpio_range bcm2712_aon_pinctrl_gpio_range = {
	.name = "aon-pinctrl-bcm2712",
	.npins = BCM2712_NUM_AON_GPIOS,
};

static const struct bcm_plat_data bcm2712_plat_data = {
	.pctl_desc = &bcm2712_pinctrl_desc,
	.gpio_range = &bcm2712_pinctrl_gpio_range,
	.pin_regs = bcm2712_gpio_pin_regs,
};

static const struct bcm_plat_data bcm2712_aon_plat_data = {
	.pctl_desc = &bcm2712_aon_pinctrl_desc,
	.gpio_range = &bcm2712_aon_pinctrl_gpio_range,
	.pin_regs = bcm2712_aon_gpio_pin_regs,
};

/* And again, this time for the 7712 variant */

static const struct pinctrl_desc bcm7712_pinctrl_desc = {
	.name = "pinctrl-bcm7712",
	.pins = bcm2712_gpio_pins,
	.npins = BCM7712_NUM_GPIOS,
	.pctlops = &bcm2712_pctl_ops,
	.pmxops = &bcm2712_pmx_ops,
	.confops = &bcm2712_pinconf_ops,
	.owner = THIS_MODULE,
};

static const struct pinctrl_desc bcm7712_aon_pinctrl_desc = {
	.name = "aon-pinctrl-bcm7712",
	.pins = bcm2712_aon_gpio_pins,
	.npins = BCM7712_NUM_AON_GPIOS,
	.pctlops = &bcm2712_pctl_ops,
	.pmxops = &bcm2712_pmx_ops,
	.confops = &bcm2712_pinconf_ops,
	.owner = THIS_MODULE,
};

static const struct pinctrl_gpio_range bcm7712_pinctrl_gpio_range = {
	.name = "pinctrl-bcm7712",
	.npins = BCM7712_NUM_GPIOS,
};

static const struct pinctrl_gpio_range bcm7712_aon_pinctrl_gpio_range = {
	.name = "aon-pinctrl-bcm7712",
	.npins = BCM7712_NUM_AON_GPIOS,
};

static const struct bcm_plat_data bcm7712_plat_data = {
	.pctl_desc = &bcm7712_pinctrl_desc,
	.gpio_range = &bcm7712_pinctrl_gpio_range,
	.pin_regs = bcm2712_gpio_pin_regs,
};

static const struct bcm_plat_data bcm7712_aon_plat_data = {
	.pctl_desc = &bcm7712_aon_pinctrl_desc,
	.gpio_range = &bcm7712_aon_pinctrl_gpio_range,
	.pin_regs = bcm2712_aon_gpio_pin_regs,
};

static const struct of_device_id bcm2712_pinctrl_match[] = {
	{
		.compatible = "brcm,bcm2712-pinctrl",
		.data = &bcm2712_plat_data,
	},
	{
		.compatible = "brcm,bcm2712-aon-pinctrl",
		.data = &bcm2712_aon_plat_data,
	},
	{
		.compatible = "brcm,bcm7712-pinctrl",
		.data = &bcm7712_plat_data,
	},
	{
		.compatible = "brcm,bcm7712-aon-pinctrl",
		.data = &bcm7712_aon_plat_data,
	},
	{}
};

static int bcm2712_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const struct bcm_plat_data *pdata;
	const struct of_device_id *match;
	struct bcm2712_pinctrl *pc;
	const char **names;
	int num_pins, i;

	BUILD_BUG_ON(ARRAY_SIZE(bcm2712_gpio_pins) < BCM2712_NUM_GPIOS);
	BUILD_BUG_ON(ARRAY_SIZE(bcm2712_aon_gpio_pins) < BCM2712_NUM_AON_GPIOS);
	BUILD_BUG_ON(ARRAY_SIZE(bcm2712_gpio_pins) < BCM7712_NUM_GPIOS);
	BUILD_BUG_ON(ARRAY_SIZE(bcm2712_aon_gpio_pins) < BCM7712_NUM_AON_GPIOS);

	match = of_match_node(bcm2712_pinctrl_match, np);
	if (!match)
		return -EINVAL;
	pdata = match->data;

	pc = devm_kzalloc(dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	platform_set_drvdata(pdev, pc);
	pc->dev = dev;
	spin_lock_init(&pc->lock);

	pc->base = devm_of_iomap(dev, np, 0, NULL);
	if (IS_ERR(pc->base)) {
		dev_err(dev, "could not get IO memory\n");
		return PTR_ERR(pc->base);
	}

	pc->pctl_desc = *pdata->pctl_desc;
	num_pins = pc->pctl_desc.npins;
	names = devm_kmalloc_array(dev, num_pins, sizeof(const char *),
				   GFP_KERNEL);
	if (!names)
		return -ENOMEM;
	for (i = 0; i < num_pins; i++)
		names[i] = pc->pctl_desc.pins[i].name;
	pc->gpio_groups = names;
	pc->pin_regs = pdata->pin_regs;
	pc->pctl_dev = devm_pinctrl_register(dev, &pc->pctl_desc, pc);
	if (IS_ERR(pc->pctl_dev))
		return PTR_ERR(pc->pctl_dev);

	pc->gpio_range = *pdata->gpio_range;
	pinctrl_add_gpio_range(pc->pctl_dev, &pc->gpio_range);

	return 0;
}

static struct platform_driver bcm2712_pinctrl_driver = {
	.probe = bcm2712_pinctrl_probe,
	.driver = {
		.name = MODULE_NAME,
		.of_match_table = bcm2712_pinctrl_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(bcm2712_pinctrl_driver);
