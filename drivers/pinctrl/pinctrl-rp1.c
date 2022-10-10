// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*
 * Driver for Raspberry Pi RP1 GPIO unit (pinctrl + GPIO)
 *
 * Copyright (C) 2019-2022 Raspberry Pi Ltd.
 *
 * This driver is inspired by:
 * pinctrl-bcm2835.c, please see original file for copyright information
 */

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <dt-bindings/pinctrl/rp1.h>

#define MODULE_NAME "pinctrl-rp1"
#define RP1_NUM_GPIOS	54
#define RP1_NUM_BANKS	3

#define RP1_FSEL_COUNT			0x1b

#define RP1_RW_OFFSET			0x0000
#define RP1_XOR_OFFSET			0x1000
#define RP1_SET_OFFSET			0x2000
#define RP1_CLR_OFFSET			0x3000

#define RP1_GPIO_STATUS			0x0000
#define RP1_GPIO_CTRL			0x0004

#define RP1_GPIO_PCIE_INTE		0x011c
#define RP1_GPIO_PCIE_INTS		0x0124

#define RP1_GPIO_EVENTS_SHIFT_RAW	20
#define RP1_GPIO_STATUS_FALLING		BIT(20)
#define RP1_GPIO_STATUS_RISING		BIT(21)
#define RP1_GPIO_STATUS_LOW		BIT(22)
#define RP1_GPIO_STATUS_HIGH		BIT(23)

#define RP1_GPIO_EVENTS_SHIFT_FILTERED	24
#define RP1_GPIO_STATUS_F_FALLING	BIT(24)
#define RP1_GPIO_STATUS_F_RISING	BIT(25)
#define RP1_GPIO_STATUS_F_LOW		BIT(26)
#define RP1_GPIO_STATUS_F_HIGH		BIT(27)

#define RP1_GPIO_CTRL_FUNCSEL_LSB	0
#define RP1_GPIO_CTRL_FUNCSEL_MASK	0x0000001f
#define RP1_GPIO_CTRL_OUTOVER_LSB	12
#define RP1_GPIO_CTRL_OUTOVER_MASK	0x00003000
#define RP1_GPIO_CTRL_OEOVER_LSB	14
#define RP1_GPIO_CTRL_OEOVER_MASK	0x0000c000
#define RP1_GPIO_CTRL_INOVER_LSB	16
#define RP1_GPIO_CTRL_INOVER_MASK	0x00030000
#define RP1_GPIO_CTRL_IRQEN_FALLING	BIT(20)
#define RP1_GPIO_CTRL_IRQEN_RISING	BIT(21)
#define RP1_GPIO_CTRL_IRQEN_LOW		BIT(22)
#define RP1_GPIO_CTRL_IRQEN_HIGH	BIT(23)
#define RP1_GPIO_CTRL_IRQEN_F_FALLING	BIT(24)
#define RP1_GPIO_CTRL_IRQEN_F_RISING	BIT(25)
#define RP1_GPIO_CTRL_IRQEN_F_LOW	BIT(26)
#define RP1_GPIO_CTRL_IRQEN_F_HIGH	BIT(27)
#define RP1_GPIO_CTRL_IRQRESET		BIT(28)
#define RP1_GPIO_CTRL_IRQOVER_LSB	30
#define RP1_GPIO_CTRL_IRQOVER_MASK	0xc0000000

#define RP1_INT_EDGE_FALLING		BIT(0)
#define RP1_INT_EDGE_RISING		BIT(1)
#define RP1_INT_LEVEL_LOW		BIT(2)
#define RP1_INT_LEVEL_HIGH		BIT(3)
#define RP1_INT_MASK			0xf

#define RP1_INT_EDGE_BOTH		(RP1_INT_EDGE_FALLING |	\
					 RP1_INT_EDGE_RISING)

#define RP1_FUNCSEL_ALT0		0x00
#define RP1_FUNCSEL_SYSRIO		0x05
#define RP1_FUNCSEL_MAX			10
#define RP1_FUNCSEL_NULL		0x1f

#define RP1_OUTOVER_PERI		0
#define RP1_OUTOVER_INVPERI		1
#define RP1_OUTOVER_LOW			2
#define RP1_OUTOVER_HIGH		3

#define RP1_OEOVER_PERI			0
#define RP1_OEOVER_INVPERI		1
#define RP1_OEOVER_DISABLE		2
#define RP1_OEOVER_ENABLE		3

#define RP1_INOVER_PERI			0
#define RP1_INOVER_INVPERI		1
#define RP1_INOVER_LOW			2
#define RP1_INOVER_HIGH			3

#define RP1_RIO_OUT			0x00
#define RP1_RIO_OE			0x04
#define RP1_RIO_IN			0x08

#define RP1_PAD_SLEWFAST_MASK		0x00000001
#define RP1_PAD_SLEWFAST_LSB		0
#define RP1_PAD_SCHMITT_MASK		0x00000002
#define RP1_PAD_SCHMITT_LSB		1
#define RP1_PAD_PULL_MASK		0x0000000c
#define RP1_PAD_PULL_LSB		2
#define RP1_PAD_DRIVE_MASK		0x00000030
#define RP1_PAD_DRIVE_LSB		4
#define RP1_PAD_IN_ENABLE_MASK		0x00000040
#define RP1_PAD_IN_ENABLE_LSB		6
#define RP1_PAD_OUT_DISABLE_MASK	0x00000080
#define RP1_PAD_OUT_DISABLE_LSB		7

#define RP1_PINCONF_PARAM_PULL		(PIN_CONFIG_END + 1)

#define FLD_GET(r, f) (((r) & (f ## _MASK)) >> (f ## _LSB))
#define FLD_SET(r, f, v) r = (((r) & ~(f ## _MASK)) | ((v) << (f ## _LSB)))

struct rp1_iobank_desc {
	int min_gpio;
	int num_gpios;
	int gpio_offset;
	int inte_offset;
	int ints_offset;
	int rio_offset;
	int pads_offset;
};

struct rp1_pin_info {
	u8 num;
	u8 bank;
	u8 offset;
	u8 fsel;
	u8 irq_type;

	void __iomem *gpio;
	void __iomem *rio;
	void __iomem *inte;
	void __iomem *ints;
	void __iomem *pad;
};

struct rp1_pinctrl {
	struct device *dev;
	void __iomem *gpio_base;
	void __iomem *rio_base;
	void __iomem *pads_base;
	int irq[RP1_NUM_BANKS];
	struct rp1_pin_info pins[RP1_NUM_GPIOS];

	struct pinctrl_dev *pctl_dev;
	struct gpio_chip gpio_chip;
	struct pinctrl_gpio_range gpio_range;

	raw_spinlock_t irq_lock[RP1_NUM_BANKS];
};

const struct rp1_iobank_desc rp1_iobanks[RP1_NUM_BANKS] = {
	/*         gpio   inte    ints     rio    pads */
	{  0, 28, 0x0000, 0x011c, 0x0124, 0x0000, 0x0004 },
	{ 28,  6, 0x4000, 0x411c, 0x4124, 0x4000, 0x4004 },
	{ 34, 20, 0x8000, 0x811c, 0x8124, 0x8000, 0x8004 },
};

/* pins are just named GPIO0..GPIO53 */
#define RP1_GPIO_PIN(a) PINCTRL_PIN(a, "gpio" #a)
static struct pinctrl_pin_desc rp1_gpio_pins[] = {
	RP1_GPIO_PIN(0),
	RP1_GPIO_PIN(1),
	RP1_GPIO_PIN(2),
	RP1_GPIO_PIN(3),
	RP1_GPIO_PIN(4),
	RP1_GPIO_PIN(5),
	RP1_GPIO_PIN(6),
	RP1_GPIO_PIN(7),
	RP1_GPIO_PIN(8),
	RP1_GPIO_PIN(9),
	RP1_GPIO_PIN(10),
	RP1_GPIO_PIN(11),
	RP1_GPIO_PIN(12),
	RP1_GPIO_PIN(13),
	RP1_GPIO_PIN(14),
	RP1_GPIO_PIN(15),
	RP1_GPIO_PIN(16),
	RP1_GPIO_PIN(17),
	RP1_GPIO_PIN(18),
	RP1_GPIO_PIN(19),
	RP1_GPIO_PIN(20),
	RP1_GPIO_PIN(21),
	RP1_GPIO_PIN(22),
	RP1_GPIO_PIN(23),
	RP1_GPIO_PIN(24),
	RP1_GPIO_PIN(25),
	RP1_GPIO_PIN(26),
	RP1_GPIO_PIN(27),
	RP1_GPIO_PIN(28),
	RP1_GPIO_PIN(29),
	RP1_GPIO_PIN(30),
	RP1_GPIO_PIN(31),
	RP1_GPIO_PIN(32),
	RP1_GPIO_PIN(33),
	RP1_GPIO_PIN(34),
	RP1_GPIO_PIN(35),
	RP1_GPIO_PIN(36),
	RP1_GPIO_PIN(37),
	RP1_GPIO_PIN(38),
	RP1_GPIO_PIN(39),
	RP1_GPIO_PIN(40),
	RP1_GPIO_PIN(41),
	RP1_GPIO_PIN(42),
	RP1_GPIO_PIN(43),
	RP1_GPIO_PIN(44),
	RP1_GPIO_PIN(45),
	RP1_GPIO_PIN(46),
	RP1_GPIO_PIN(47),
	RP1_GPIO_PIN(48),
	RP1_GPIO_PIN(49),
	RP1_GPIO_PIN(50),
	RP1_GPIO_PIN(51),
	RP1_GPIO_PIN(52),
	RP1_GPIO_PIN(53),
};

/* one pin per group */
static const char * const rp1_gpio_groups[] = {
	"gpio0",
	"gpio1",
	"gpio2",
	"gpio3",
	"gpio4",
	"gpio5",
	"gpio6",
	"gpio7",
	"gpio8",
	"gpio9",
	"gpio10",
	"gpio11",
	"gpio12",
	"gpio13",
	"gpio14",
	"gpio15",
	"gpio16",
	"gpio17",
	"gpio18",
	"gpio19",
	"gpio20",
	"gpio21",
	"gpio22",
	"gpio23",
	"gpio24",
	"gpio25",
	"gpio26",
	"gpio27",
	"gpio28",
	"gpio29",
	"gpio30",
	"gpio31",
	"gpio32",
	"gpio33",
	"gpio34",
	"gpio35",
	"gpio36",
	"gpio37",
	"gpio38",
	"gpio39",
	"gpio40",
	"gpio41",
	"gpio42",
	"gpio43",
	"gpio44",
	"gpio45",
	"gpio46",
	"gpio47",
	"gpio48",
	"gpio49",
	"gpio50",
	"gpio51",
	"gpio52",
	"gpio53",
};

static const int legacy_fsel_map[][6] = {
	{  3, -1,  1, -1,  2, -1 },
	{  3, -1,  1, -1,  2, -1 },
	{  3, -1,  1, -1,  2, -1 },
	{  3, -1,  1, -1,  2, -1 },
	{  0, -1,  1, -1,  2,  3 },
	{  0, -1,  1, -1,  2,  3 },
	{  0, -1,  1, -1,  2,  3 },
	{  0, -1,  1, -1,  2,  3 },
	{  0, -1,  1, -1,  2, -1 },
	{  0, -1,  1, -1,  2, -1 },
	{  0, -1,  1, -1,  2, -1 },
	{  0, -1,  1, -1,  2, -1 },
	{  0, -1,  1, -1,  2, -1 },
	{  0, -1,  1, -1,  2, -1 },
	{  4, -1,  1, -1,  2, -1 },
	{  4, -1,  1, -1,  2, -1 },

	{ -1, -1,  1,  4,  0, -1 },
	{ -1, -1,  1,  4,  0, -1 },
	{  2, -1,  1, -1,  0, -1 },
	{  2, -1,  1, -1,  0, -1 },
	{  2, -1,  1, -1,  0, -1 },
	{  2, -1,  1, -1,  0, -1 },
	{  0, -1,  1, -1, -1, -1 },
	{  0, -1,  1, -1, -1, -1 },
	{  0, -1,  1, -1, -1, -1 },
	{  0, -1,  1, -1, -1, -1 },
	{  0, -1,  1, -1, -1, -1 },
	{  0, -1,  1, -1, -1, -1 },
};

static const char * const rp1_functions[RP1_FSEL_COUNT] = {
	[RP1_FSEL_GPIO_IN] = "gpio_in",
	[RP1_FSEL_GPIO_OUT] = "gpio_out",
	[RP1_FSEL_ALT0_LEGACY] = "alt0_legacy",
	[RP1_FSEL_ALT1_LEGACY] = "alt1_legacy",
	[RP1_FSEL_ALT2_LEGACY] = "alt2_legacy",
	[RP1_FSEL_ALT3_LEGACY] = "alt3_legacy",
	[RP1_FSEL_ALT4_LEGACY] = "alt4_legacy",
	[RP1_FSEL_ALT5_LEGACY] = "alt5_legacy",
	[RP1_FSEL_ALT0] = "alt0",
	[RP1_FSEL_ALT1] = "alt1",
	[RP1_FSEL_ALT2] = "alt2",
	[RP1_FSEL_ALT3] = "alt3",
	[RP1_FSEL_ALT4] = "alt4",
	[RP1_FSEL_ALT5] = "alt5",
	[RP1_FSEL_ALT6] = "alt6",
	[RP1_FSEL_ALT7] = "alt7",
	[RP1_FSEL_ALT8] = "alt8",
	[RP1_FSEL_ALT0INV] = "alt0inv",
	[RP1_FSEL_ALT1INV] = "alt1inv",
	[RP1_FSEL_ALT2INV] = "alt2inv",
	[RP1_FSEL_ALT3INV] = "alt3inv",
	[RP1_FSEL_ALT4INV] = "alt4inv",
	[RP1_FSEL_ALT5INV] = "alt5inv",
	[RP1_FSEL_ALT6INV] = "alt6inv",
	[RP1_FSEL_ALT7INV] = "alt7inv",
	[RP1_FSEL_ALT8INV] = "alt8inv",
	[RP1_FSEL_NONE] = "none",
};

static const char * const irq_type_names[] = {
	[IRQ_TYPE_NONE] = "none",
	[IRQ_TYPE_EDGE_RISING] = "edge-rising",
	[IRQ_TYPE_EDGE_FALLING] = "edge-falling",
	[IRQ_TYPE_EDGE_BOTH] = "edge-both",
	[IRQ_TYPE_LEVEL_HIGH] = "level-high",
	[IRQ_TYPE_LEVEL_LOW] = "level-low",
};

static int rp1_pinconf_set(struct pinctrl_dev *pctldev,
			  unsigned int offset, unsigned long *configs,
			  unsigned int num_configs);

static struct rp1_pin_info *rp1_get_pin(struct gpio_chip *chip,
				      unsigned int offset)
{
	struct rp1_pinctrl *pc = gpiochip_get_data(chip);
	if (pc && offset < RP1_NUM_GPIOS)
		return &pc->pins[offset];
	return NULL;
}

static struct rp1_pin_info *rp1_get_pin_pctl(struct pinctrl_dev *pctldev,
					   unsigned int offset)
{
	struct rp1_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	if (pc && offset < RP1_NUM_GPIOS)
		return &pc->pins[offset];
	return NULL;
}

static void rp1_input_enable(struct rp1_pin_info *pin, int value)
{
	u32 padctrl = readl(pin->pad);

	FLD_SET(padctrl, RP1_PAD_IN_ENABLE, !!value);

	writel(padctrl, pin->pad);
}

static void rp1_output_enable(struct rp1_pin_info *pin, int value)
{
	u32 padctrl = readl(pin->pad);

	FLD_SET(padctrl, RP1_PAD_OUT_DISABLE, !value);

	writel(padctrl, pin->pad);
}

static inline u32 rp1_get_fsel(struct rp1_pin_info *pin)
{
	u32 ctrl = readl(pin->gpio + RP1_GPIO_CTRL);
	u32 outover = FLD_GET(ctrl, RP1_GPIO_CTRL_OUTOVER);
	u32 funcsel = FLD_GET(ctrl, RP1_GPIO_CTRL_FUNCSEL);
	u32 fsel = -EINVAL;

	if (funcsel == RP1_FUNCSEL_SYSRIO) {
		/* An input or an output */
		fsel = (readl(pin->rio + RP1_RIO_OE) & (1 << pin->offset)) ?
			RP1_FSEL_GPIO_OUT : RP1_FSEL_GPIO_IN;
	} else if (funcsel <= RP1_FUNCSEL_MAX) {
		fsel = (outover == RP1_OUTOVER_INVPERI ? RP1_FSEL_ALT0INV :
			RP1_FSEL_ALT0) +
			funcsel * 2;
	} else if (funcsel == RP1_FUNCSEL_NULL) {
		fsel = RP1_FSEL_NONE;
	}

	pr_debug("get_fsel %d: %08x - %d (%s)\n", pin->num, ctrl, fsel,
		 (fsel >= 0) ? rp1_functions[fsel] : "invalid");

	return fsel;
}

static inline void rp1_set_fsel(struct rp1_pin_info *pin, u32 fsel)
{
	u32 ctrl = readl(pin->gpio + RP1_GPIO_CTRL);
	int remap_fsel_idx;
	u32 cur;

	pr_debug("set_fsel %d: %d (%s)\n", pin->num, fsel,
		 rp1_functions[fsel]);

	rp1_input_enable(pin, 1);
	rp1_output_enable(pin, 1);

	cur = rp1_get_fsel(pin);

	switch (fsel) {
	case RP1_FSEL_ALT0_LEGACY: remap_fsel_idx = 0; break;
	case RP1_FSEL_ALT1_LEGACY: remap_fsel_idx = 1; break;
	case RP1_FSEL_ALT2_LEGACY: remap_fsel_idx = 2; break;
	case RP1_FSEL_ALT3_LEGACY: remap_fsel_idx = 3; break;
	case RP1_FSEL_ALT4_LEGACY: remap_fsel_idx = 4; break;
	case RP1_FSEL_ALT5_LEGACY: remap_fsel_idx = 5; break;
	default: remap_fsel_idx = -1; break;
	}

	if (remap_fsel_idx >= 0) {
		if (pin->num < ARRAY_SIZE(legacy_fsel_map))
			fsel = legacy_fsel_map[pin->num][remap_fsel_idx];
		else
			fsel = remap_fsel_idx;
		fsel += RP1_FSEL_ALT0;
	}

	if (cur == fsel)
		return;

	/* always transition through GPIO_IN */
	if (cur != RP1_FSEL_GPIO_IN && fsel != RP1_FSEL_GPIO_IN) {
		FLD_SET(ctrl, RP1_GPIO_CTRL_OEOVER, RP1_OEOVER_DISABLE);
		pr_debug("  trans %d: %08x\n", pin->num, ctrl);
		writel(ctrl, pin->gpio + RP1_GPIO_CTRL);
	}

	FLD_SET(ctrl, RP1_GPIO_CTRL_OEOVER, RP1_OEOVER_PERI);

	if (fsel == RP1_FSEL_GPIO_IN) {
		writel(1 << pin->offset, pin->rio + RP1_RIO_OE + RP1_CLR_OFFSET);
		FLD_SET(ctrl, RP1_GPIO_CTRL_OUTOVER, RP1_OUTOVER_PERI);
		FLD_SET(ctrl, RP1_GPIO_CTRL_FUNCSEL, RP1_FUNCSEL_SYSRIO);
	} else if (fsel == RP1_FSEL_GPIO_OUT) {
		writel(1 << pin->offset, pin->rio + RP1_RIO_OE + RP1_SET_OFFSET);
		FLD_SET(ctrl, RP1_GPIO_CTRL_OUTOVER, RP1_OUTOVER_PERI);
		FLD_SET(ctrl, RP1_GPIO_CTRL_FUNCSEL, RP1_FUNCSEL_SYSRIO);
	} else if (fsel >= RP1_FSEL_ALT0 && fsel <= RP1_FSEL_NONE) {
		if (fsel & 0x1)
			FLD_SET(ctrl, RP1_GPIO_CTRL_OUTOVER, RP1_OUTOVER_INVPERI);
		FLD_SET(ctrl, RP1_GPIO_CTRL_FUNCSEL,
			RP1_FUNCSEL_ALT0 + (fsel - RP1_FSEL_ALT0)/2);
	} else if (fsel == RP1_FSEL_NONE) {
		FLD_SET(ctrl, RP1_GPIO_CTRL_FUNCSEL, RP1_FUNCSEL_NULL);
	}
	writel(ctrl, pin->gpio + RP1_GPIO_CTRL);
	pr_debug("  write %d: %08x\n", pin->num, ctrl);
}

static int rp1_get_value(struct rp1_pin_info *pin)
{
	return !!(readl(pin->rio + RP1_RIO_IN) & (1 << pin->offset));
}

static void rp1_set_value(struct rp1_pin_info *pin, int value)
{
	/* Assume the pin is already an output */
	writel(1 << pin->offset,
	       pin->rio + RP1_RIO_OUT + (value ? RP1_SET_OFFSET : RP1_CLR_OFFSET));
}

static int rp1_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct rp1_pin_info *pin = rp1_get_pin(chip, offset);

	if (!pin)
		return -EINVAL;
	rp1_set_fsel(pin, RP1_FSEL_GPIO_IN);
	return 0;
}

static int rp1_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct rp1_pin_info *pin = rp1_get_pin(chip, offset);
	int ret;

	if (!pin)
		return -EINVAL;
	ret = rp1_get_value(pin);
	pr_debug("rp1_gpio_get(%d) -> %d\n", offset, ret);
	return ret;
}

static int rp1_gpio_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	struct rp1_pin_info *pin = rp1_get_pin(chip, offset);
	u32 fsel;

	if (!pin)
		return -EINVAL;
	fsel = rp1_get_fsel(pin);
	return (fsel <= RP1_FSEL_GPIO_OUT) ? (fsel == RP1_FSEL_GPIO_IN) : -EINVAL;
}

static void rp1_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct rp1_pin_info *pin = rp1_get_pin(chip, offset);

	pr_debug("rp1_gpio_set(%d, %d)\n", offset, value);
	if (pin)
		rp1_set_value(pin, value);
}

static int rp1_gpio_direction_output(struct gpio_chip *chip,
		unsigned offset, int value)
{
	struct rp1_pin_info *pin = rp1_get_pin(chip, offset);

	pr_debug("rp1_gpio_direction_output(%d, %d)\n", offset, value);
	if (!pin)
		return -EINVAL;
	rp1_set_value(pin, value);
	rp1_set_fsel(pin, RP1_FSEL_GPIO_OUT);
	return 0;
}

static int rp1_gpio_set_config(struct gpio_chip *gc, unsigned offset,
			      unsigned long config)
{
	struct rp1_pinctrl *pc = gpiochip_get_data(gc);
	unsigned long configs[] = { config };

	return rp1_pinconf_set(pc->pctl_dev, offset, configs,
			      ARRAY_SIZE(configs));
}

static const struct gpio_chip rp1_gpio_chip = {
	.label = MODULE_NAME,
	.owner = THIS_MODULE,
	.request = gpiochip_generic_request,
	.free = gpiochip_generic_free,
	.direction_input = rp1_gpio_direction_input,
	.direction_output = rp1_gpio_direction_output,
	.get_direction = rp1_gpio_get_direction,
	.get = rp1_gpio_get,
	.set = rp1_gpio_set,
	.base = -1,
	.set_config = rp1_gpio_set_config,
	.ngpio = RP1_NUM_GPIOS,
	.can_sleep = false,
};

static void rp1_gpio_irq_handler(struct irq_desc *desc)
{
	struct gpio_chip *chip = irq_desc_get_handler_data(desc);
	struct rp1_pinctrl *pc = gpiochip_get_data(chip);
	struct irq_chip *host_chip = irq_desc_get_chip(desc);
	const struct rp1_iobank_desc *bank;
	int irq = irq_desc_get_irq(desc);
	unsigned long ints;
	int b;

	if (pc->irq[0] == irq)
		bank = &rp1_iobanks[0];
	else if (pc->irq[1] == irq)
		bank = &rp1_iobanks[1];
	else
		bank = &rp1_iobanks[2];

	chained_irq_enter(host_chip, desc);

	ints = readl(pc->gpio_base + bank->ints_offset);
	for_each_set_bit(b, &ints, 32) {
		struct rp1_pin_info *pin = rp1_get_pin(chip, b);

		writel(RP1_GPIO_CTRL_IRQRESET,
		       pin->gpio + RP1_SET_OFFSET + RP1_GPIO_CTRL);
		generic_handle_irq(irq_linear_revmap(pc->gpio_chip.irq.domain,
						     bank->gpio_offset + b));
	}

	chained_irq_exit(host_chip, desc);
}

static void rp1_gpio_irq_config(struct rp1_pin_info *pin, bool enable)
{
	writel(1 << pin->offset,
	       pin->inte + (enable ? RP1_SET_OFFSET : RP1_CLR_OFFSET));
	if (!enable)
		/* Clear any latched events */
		writel(RP1_GPIO_CTRL_IRQRESET,
		       pin->gpio + RP1_SET_OFFSET + RP1_GPIO_CTRL);
}

static void rp1_gpio_irq_enable(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	unsigned gpio = irqd_to_hwirq(data);
	struct rp1_pin_info *pin = rp1_get_pin(chip, gpio);

	rp1_gpio_irq_config(pin, true);
}

static void rp1_gpio_irq_disable(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	unsigned gpio = irqd_to_hwirq(data);
	struct rp1_pin_info *pin = rp1_get_pin(chip, gpio);

	rp1_gpio_irq_config(pin, false);
}

static int rp1_irq_set_type(struct rp1_pin_info *pin, unsigned int type)
{
	u32 irq_flags;

	switch (type) {
	case IRQ_TYPE_NONE:
		irq_flags = 0;
		break;
	case IRQ_TYPE_EDGE_RISING:
		irq_flags = RP1_INT_EDGE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		irq_flags = RP1_INT_EDGE_FALLING;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		irq_flags = RP1_INT_EDGE_RISING | RP1_INT_EDGE_FALLING;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		irq_flags = RP1_INT_LEVEL_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		irq_flags = RP1_INT_LEVEL_LOW;
		break;

	default:
		return -EINVAL;
	}

	/* Clear them all */
	writel(RP1_INT_MASK << RP1_GPIO_EVENTS_SHIFT_RAW,
	       pin->gpio + RP1_CLR_OFFSET + RP1_GPIO_CTRL);
	/* Set those that are needed */
	writel(irq_flags << RP1_GPIO_EVENTS_SHIFT_RAW,
	       pin->gpio + RP1_SET_OFFSET + RP1_GPIO_CTRL);
	pin->irq_type = type;

	return 0;
}

static int rp1_gpio_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	struct rp1_pinctrl *pc = gpiochip_get_data(chip);
	unsigned gpio = irqd_to_hwirq(data);
	struct rp1_pin_info *pin = rp1_get_pin(chip, gpio);
	int bank = pin->bank;
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&pc->irq_lock[bank], flags);

	ret = rp1_irq_set_type(pin, type);
	if (!ret) {
		if (type & IRQ_TYPE_EDGE_BOTH)
			irq_set_handler_locked(data, handle_edge_irq);
		else
			irq_set_handler_locked(data, handle_level_irq);
	}

	raw_spin_unlock_irqrestore(&pc->irq_lock[bank], flags);

	return ret;
}

static void rp1_gpio_irq_ack(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	unsigned gpio = irqd_to_hwirq(data);
	struct rp1_pin_info *pin = rp1_get_pin(chip, gpio);

	/* Clear any latched events */
	writel(RP1_GPIO_CTRL_IRQRESET, pin->gpio + RP1_SET_OFFSET + RP1_GPIO_CTRL);
}

static struct irq_chip rp1_gpio_irq_chip = {
	.name = MODULE_NAME,
	.irq_enable = rp1_gpio_irq_enable,
	.irq_disable = rp1_gpio_irq_disable,
	.irq_set_type = rp1_gpio_irq_set_type,
	.irq_ack = rp1_gpio_irq_ack,
	.irq_mask = rp1_gpio_irq_disable,
	.irq_unmask = rp1_gpio_irq_enable,
	.flags = IRQCHIP_IMMUTABLE,
};

static int rp1_pctl_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(rp1_gpio_groups);
}

static const char *rp1_pctl_get_group_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	return rp1_gpio_groups[selector];
}

static int rp1_pctl_get_group_pins(struct pinctrl_dev *pctldev,
		unsigned selector,
		const unsigned **pins,
		unsigned *num_pins)
{
	*pins = &rp1_gpio_pins[selector].number;
	*num_pins = 1;

	return 0;
}

static void rp1_pctl_pin_dbg_show(struct pinctrl_dev *pctldev,
		struct seq_file *s,
		unsigned offset)
{
	struct rp1_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	struct gpio_chip *chip = &pc->gpio_chip;
	struct rp1_pin_info *pin = rp1_get_pin_pctl(pctldev, offset);
	u32 fsel = rp1_get_fsel(pin);
	const char *fname = rp1_functions[fsel];
	int value = rp1_get_value(pin);
	int irq = irq_find_mapping(chip->irq.domain, offset);

	seq_printf(s, "function %s in %s; irq %d (%s)",
		fname, value ? "hi" : "lo",
		irq, irq_type_names[pin->irq_type]);
}

static void rp1_pctl_dt_free_map(struct pinctrl_dev *pctldev,
		struct pinctrl_map *maps, unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++)
		if (maps[i].type == PIN_MAP_TYPE_CONFIGS_PIN)
			kfree(maps[i].data.configs.configs);

	kfree(maps);
}

static int rp1_pctl_dt_node_to_map_func(struct rp1_pinctrl *pc,
		struct device_node *np, u32 pin, u32 fnum,
		struct pinctrl_map **maps)
{
	struct pinctrl_map *map = *maps;

	if (fnum >= ARRAY_SIZE(rp1_functions)) {
		dev_err(pc->dev, "%pOF: invalid brcm,function %d\n", np, fnum);
		return -EINVAL;
	}

	map->type = PIN_MAP_TYPE_MUX_GROUP;
	map->data.mux.group = rp1_gpio_groups[pin];
	map->data.mux.function = rp1_functions[fnum];
	(*maps)++;

	return 0;
}

static int rp1_pctl_dt_node_to_map_pull(struct rp1_pinctrl *pc,
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
	configs[0] = pinconf_to_config_packed(RP1_PINCONF_PARAM_PULL, pull);

	map->type = PIN_MAP_TYPE_CONFIGS_PIN;
	map->data.configs.group_or_pin = rp1_gpio_pins[pin].name;
	map->data.configs.configs = configs;
	map->data.configs.num_configs = 1;
	(*maps)++;

	return 0;
}

static int rp1_pctl_dt_node_to_map(struct pinctrl_dev *pctldev,
		struct device_node *np,
		struct pinctrl_map **map, unsigned int *num_maps)
{
	struct rp1_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
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
		if (pin >= ARRAY_SIZE(rp1_gpio_pins)) {
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
			err = rp1_pctl_dt_node_to_map_func(pc, np, pin,
							func, &cur_map);
			if (err)
				goto out;
		}
		if (num_pulls) {
			err = of_property_read_u32_index(np, "brcm,pull",
					(num_pulls > 1) ? i : 0, &pull);
			if (err)
				goto out;
			err = rp1_pctl_dt_node_to_map_pull(pc, np, pin,
							pull, &cur_map);
			if (err)
				goto out;
		}
	}

	*map = maps;
	*num_maps = num_pins * maps_per_pin;

	return 0;

out:
	rp1_pctl_dt_free_map(pctldev, maps, num_pins * maps_per_pin);
	return err;
}

static const struct pinctrl_ops rp1_pctl_ops = {
	.get_groups_count = rp1_pctl_get_groups_count,
	.get_group_name = rp1_pctl_get_group_name,
	.get_group_pins = rp1_pctl_get_group_pins,
	.pin_dbg_show = rp1_pctl_pin_dbg_show,
	.dt_node_to_map = rp1_pctl_dt_node_to_map,
	.dt_free_map = rp1_pctl_dt_free_map,
};

static int rp1_pmx_free(struct pinctrl_dev *pctldev,
		       unsigned offset)
{
	struct rp1_pin_info *pin = rp1_get_pin_pctl(pctldev, offset);
	u32 fsel = rp1_get_fsel(pin);

	/* Return non-GPIOs to GPIO_IN */
	if (fsel != RP1_FSEL_GPIO_IN && fsel != RP1_FSEL_GPIO_OUT)
		rp1_set_fsel(pin, RP1_FSEL_GPIO_IN);

	return 0;
}

static int rp1_pmx_get_functions_count(struct pinctrl_dev *pctldev)
{
	return RP1_FSEL_COUNT;
}

static const char *rp1_pmx_get_function_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	return rp1_functions[selector];
}

static int rp1_pmx_get_function_groups(struct pinctrl_dev *pctldev,
		unsigned selector,
		const char * const **groups,
		unsigned * const num_groups)
{
	/* every pin can do every function */
	*groups = rp1_gpio_groups;
	*num_groups = ARRAY_SIZE(rp1_gpio_groups);

	return 0;
}

static int rp1_pmx_set(struct pinctrl_dev *pctldev,
		unsigned func_selector,
		unsigned group_selector)
{
	struct rp1_pin_info *pin = rp1_get_pin_pctl(pctldev, group_selector);

	rp1_set_fsel(pin, func_selector);

	return 0;
}

static void rp1_pmx_gpio_disable_free(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range,
		unsigned offset)
{
	struct rp1_pin_info *pin = rp1_get_pin_pctl(pctldev, offset);

	/* disable by setting to GPIO_IN */
	rp1_set_fsel(pin, RP1_FSEL_GPIO_IN);
}

static int rp1_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range,
		unsigned offset,
		bool input)
{
	struct rp1_pin_info *pin = rp1_get_pin_pctl(pctldev, offset);
	u32 fsel = input ? RP1_FSEL_GPIO_IN : RP1_FSEL_GPIO_OUT;

	rp1_set_fsel(pin, fsel);

	return 0;
}

static const struct pinmux_ops rp1_pmx_ops = {
	.free = rp1_pmx_free,
	.get_functions_count = rp1_pmx_get_functions_count,
	.get_function_name = rp1_pmx_get_function_name,
	.get_function_groups = rp1_pmx_get_function_groups,
	.set_mux = rp1_pmx_set,
	.gpio_disable_free = rp1_pmx_gpio_disable_free,
	.gpio_set_direction = rp1_pmx_gpio_set_direction,
};

static void rp1_pull_config_set(struct rp1_pin_info *pin, unsigned int arg)
{
	u32 padctrl = readl(pin->pad);

	FLD_SET(padctrl, RP1_PAD_PULL, arg & 0x3);

	writel(padctrl, pin->pad);
}

/* Generic pinconf methods */

static int rp1_pinconf_set(struct pinctrl_dev *pctldev,
			unsigned int offset, unsigned long *configs,
			unsigned int num_configs)
{
	struct rp1_pin_info *pin = rp1_get_pin_pctl(pctldev, offset);
	u32 param, arg;
	int i;

	pr_debug("rp1_pinconf_set(%d)\n", offset);
	if (!pin)
		return -EINVAL;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		/* Set legacy brcm,pull */
		case RP1_PINCONF_PARAM_PULL:
			rp1_pull_config_set(pin, arg);
			break;

		/* Set pull generic bindings */
		case PIN_CONFIG_BIAS_DISABLE:
			rp1_pull_config_set(pin, RP1_PUD_OFF);
			break;

		case PIN_CONFIG_BIAS_PULL_DOWN:
			rp1_pull_config_set(pin, RP1_PUD_DOWN);
			break;

		case PIN_CONFIG_BIAS_PULL_UP:
			rp1_pull_config_set(pin, RP1_PUD_UP);
			break;

		case PIN_CONFIG_INPUT_ENABLE:
			rp1_input_enable(pin, arg);
			break;

		case PIN_CONFIG_OUTPUT_ENABLE:
			rp1_output_enable(pin, arg);
			break;

		/* Set output-high or output-low */
		case PIN_CONFIG_OUTPUT:
			rp1_set_fsel(pin, RP1_FSEL_GPIO_OUT);
			rp1_set_value(pin, arg);
			break;

		default:
			return -ENOTSUPP;

		} /* switch param type */
	} /* for each config */

	return 0;
}

static int rp1_pinconf_get(struct pinctrl_dev *pctldev,
			  unsigned offset, unsigned long *config)
{
	struct rp1_pin_info *pin = rp1_get_pin_pctl(pctldev, offset);
	enum pin_config_param param = pinconf_to_config_param(*config);
	u32 arg = pinconf_to_config_argument(*config);

	pr_debug("rp1_pinconf_get(%d)\n", offset);
	if (!pin)
		return -EINVAL;
	switch (param) {
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT_ENABLE:
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_DRIVE_STRENGTH:
	case PIN_CONFIG_SLEW_RATE:
		// XXX Do something clever
		// break;
		(void)arg;
		(void)pin;
		return -ENOTSUPP;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static const struct pinconf_ops rp1_pinconf_ops = {
	.is_generic = true,
	.pin_config_get = rp1_pinconf_get,
	.pin_config_set = rp1_pinconf_set,
};

static struct pinctrl_desc rp1_pinctrl_desc = {
	.name = MODULE_NAME,
	.pins = rp1_gpio_pins,
	.npins = ARRAY_SIZE(rp1_gpio_pins),
	.pctlops = &rp1_pctl_ops,
	.pmxops = &rp1_pmx_ops,
	.confops = &rp1_pinconf_ops,
	.owner = THIS_MODULE,
};

static struct pinctrl_gpio_range rp1_pinctrl_gpio_range = {
	.name = MODULE_NAME,
	.npins = RP1_NUM_GPIOS,
};

static const struct of_device_id rp1_pinctrl_match[] = {
	{
		.compatible = "raspberrypi,rp1-gpio",
		.data = &rp1_pinconf_ops,
	},
	{}
};

static inline void __iomem *devm_auto_iomap(struct platform_device *pdev,
					    unsigned int index)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	if (np)
		return devm_of_iomap(dev, np, (int)index, NULL);
	else
		return devm_platform_ioremap_resource(pdev, index);
}

static int rp1_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rp1_pinctrl *pc;
	struct gpio_irq_chip *girq;
	int err, i;
	BUILD_BUG_ON(ARRAY_SIZE(rp1_gpio_pins) != RP1_NUM_GPIOS);
	BUILD_BUG_ON(ARRAY_SIZE(rp1_gpio_groups) != RP1_NUM_GPIOS);

	pc = devm_kzalloc(dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	platform_set_drvdata(pdev, pc);
	pc->dev = dev;

	pc->gpio_base = devm_auto_iomap(pdev, 0);
	if (IS_ERR(pc->gpio_base)) {
		dev_err(dev, "could not get GPIO IO memory\n");
		return PTR_ERR(pc->gpio_base);
	}

	pc->rio_base = devm_auto_iomap(pdev, 1);
	if (IS_ERR(pc->rio_base)) {
		dev_err(dev, "could not get RIO IO memory\n");
		return PTR_ERR(pc->rio_base);
	}

	pc->pads_base = devm_auto_iomap(pdev, 2);
	if (IS_ERR(pc->pads_base)) {
		dev_err(dev, "could not get PADS IO memory\n");
		return PTR_ERR(pc->pads_base);
	}

	pc->gpio_chip = rp1_gpio_chip;
	pc->gpio_chip.parent = dev;
	pc->gpio_chip.of_node = np;

	for (i = 0; i < RP1_NUM_BANKS; i++) {
		const struct rp1_iobank_desc *bank = &rp1_iobanks[i];
		int j;

		for (j = 0; j < bank->num_gpios; j++) {
			struct rp1_pin_info *pin =
				&pc->pins[bank->min_gpio + j];

			pin->num = bank->min_gpio + j;
			pin->bank = i;
			pin->offset = j;

			pin->gpio = pc->gpio_base + bank->gpio_offset +
				    j * sizeof(u32) * 2;
			pin->inte = pc->gpio_base + bank->inte_offset;
			pin->ints = pc->gpio_base + bank->ints_offset;
			pin->rio  = pc->rio_base + bank->rio_offset;
			pin->pad  = pc->pads_base + bank->pads_offset +
				    j * sizeof(u32);
		}

		raw_spin_lock_init(&pc->irq_lock[i]);
	}

	pc->pctl_dev = devm_pinctrl_register(dev, &rp1_pinctrl_desc, pc);
	if (IS_ERR(pc->pctl_dev))
		return PTR_ERR(pc->pctl_dev);

	girq = &pc->gpio_chip.irq;
	girq->chip = &rp1_gpio_irq_chip;
	girq->parent_handler = rp1_gpio_irq_handler;
	girq->num_parents = RP1_NUM_BANKS;
	girq->parents = pc->irq;

	/*
	 * Use the same handler for all groups: this is necessary
	 * since we use one gpiochip to cover all lines - the
	 * irq handler then needs to figure out which group and
	 * bank that was firing the IRQ and look up the per-group
	 * and bank data.
	 */
	for (i = 0; i < RP1_NUM_BANKS; i++) {
		pc->irq[i] = irq_of_parse_and_map(np, i);
		if (!pc->irq[i]) {
			girq->num_parents = i;
			break;
		}
	}

	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_level_irq;

	err = devm_gpiochip_add_data(dev, &pc->gpio_chip, pc);
	if (err) {
		dev_err(dev, "could not add GPIO chip\n");
		return err;
	}

	pc->gpio_range = rp1_pinctrl_gpio_range;
	pc->gpio_range.base = pc->gpio_chip.base;
	pc->gpio_range.gc = &pc->gpio_chip;
	pinctrl_add_gpio_range(pc->pctl_dev, &pc->gpio_range);

	return 0;
}

static struct platform_driver rp1_pinctrl_driver = {
	.probe = rp1_pinctrl_probe,
	.driver = {
		.name = MODULE_NAME,
		.of_match_table = rp1_pinctrl_match,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(rp1_pinctrl_driver);
