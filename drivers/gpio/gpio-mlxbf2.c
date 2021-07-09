// SPDX-License-Identifier: GPL-2.0-only or BSD-3-Clause

/*
 *  Copyright (c) 2020-2021 NVIDIA Corporation.
 */

#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/resource.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define DRV_VERSION "1.2"

/*
 * There are 3 YU GPIO blocks:
 * yu.gpio[0]: HOST_GPIO0->HOST_GPIO31
 * yu.gpio[1]: HOST_GPIO32->HOST_GPIO63
 * yu.gpio[2]: HOST_GPIO64->HOST_GPIO69
 * yu.gpio[16]: HOST_GPIO70
 */
#define MLXBF2_GPIO_MAX_PINS_PER_BLOCK 32

typedef enum {
	GPIO_BLOCK0 = 0,
	GPIO_BLOCK1 = 1,
	GPIO_BLOCK2 = 2,
	GPIO_BLOCK16 = 16
} yu_gpio_block;

/*
 * arm_gpio_lock register:
 * bit[31]	lock status: active if set
 * bit[15:0]	set lock
 * The lock is enabled only if 0xd42f is written to this field
 */
#define YU_ARM_GPIO_LOCK_ADDR		0x2801088
#define YU_ARM_GPIO_LOCK_SIZE		0x8
#define YU_LOCK_ACTIVE_BIT(val)		(val >> 31)
#define YU_ARM_GPIO_LOCK_ACQUIRE	0xd42f
#define YU_ARM_GPIO_LOCK_RELEASE	0x0

#define YU_CAUSE_GPIO_ADDR		0x2801530
#define YU_CAUSE_GPIO_ADDR_SIZE		0x4

/*
 * gpio[x] block registers and their offset
 */
#define YU_GPIO_DATAIN			0x04
#define YU_GPIO_MODE1			0x08
#define YU_GPIO_MODE0			0x0c
#define YU_GPIO_DATASET			0x14
#define YU_GPIO_DATACLEAR		0x18
#define YU_GPIO_FUNCTIONAL_ENABLE1	0x24
#define YU_GPIO_FUNCTIONAL_ENABLE0	0x28
#define YU_GPIO_CAUSE_RISE_EN		0x44
#define YU_GPIO_CAUSE_FALL_EN		0x48
#define YU_GPIO_MODE1_CLEAR		0x50
#define YU_GPIO_MODE0_SET		0x54
#define YU_GPIO_MODE0_CLEAR		0x58

#define YU_GPIO_CAUSE_OR_CAUSE_EVTEN0	0x80
#define YU_GPIO_CAUSE_OR_EVTEN0		0x94
#define YU_GPIO_CAUSE_OR_CLRCAUSE	0x98

#ifdef CONFIG_PM
struct mlxbf2_gpio_context_save_regs {
	u32 gpio_mode0;
	u32 gpio_mode1;
};
#endif

/* BlueField-2 gpio block context structure. */
struct mlxbf2_gpio_context {
	struct gpio_chip gc;
	struct irq_chip irq_chip;

	/* YU GPIO blocks address */
	void __iomem *gpio_io;

	/* YU cause gpio arm coalesce0 address */
	void __iomem *cause_gpio_arm_coalesce0_io;

	/* YU GPIO pin responsible for low power mode */
	unsigned long low_pwr_pin;

	/* YU GPIO pin responsible for soft reset */
	unsigned long rst_pin;

	/* YU GPIO pin connected to PHY INT_N signal */
	unsigned long phy_int_pin;

	/* YU GPIO block interrupt mask */
	u32 gpio_int_mask;

	/* Worker function */
	struct work_struct send_work;

#ifdef CONFIG_PM
	struct mlxbf2_gpio_context_save_regs *csave_regs;
#endif
};

/* BlueField-2 gpio shared structure. */
struct mlxbf2_gpio_param {
	void __iomem *io;
	struct resource *res;
	struct mutex *lock;
};

static struct resource yu_arm_gpio_lock_res = {
	.start = YU_ARM_GPIO_LOCK_ADDR,
	.end   = YU_ARM_GPIO_LOCK_ADDR + YU_ARM_GPIO_LOCK_SIZE - 1,
	.name  = "YU_ARM_GPIO_LOCK",
};

static DEFINE_MUTEX(yu_arm_gpio_lock_mutex);

static struct mlxbf2_gpio_param yu_arm_gpio_lock_param = {
	.res = &yu_arm_gpio_lock_res,
	.lock = &yu_arm_gpio_lock_mutex,
};

static struct resource yu_cause_gpio_res = {
	.start = YU_CAUSE_GPIO_ADDR,
	.end   = YU_CAUSE_GPIO_ADDR + YU_CAUSE_GPIO_ADDR_SIZE - 1,
	.name  = "YU_CAUSE_GPIO",
};

static DEFINE_MUTEX(yu_cause_gpio_mutex);

static struct mlxbf2_gpio_param yu_cause_gpio_param = {
	.res = &yu_cause_gpio_res,
	.lock = &yu_cause_gpio_mutex,
};

/* Request memory region and map yu_arm_gpio_lock resource */
static int mlxbf2_gpio_get_lock_res(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	resource_size_t size;
	int ret = 0;

	mutex_lock(yu_arm_gpio_lock_param.lock);

	/* Check if the memory map already exists */
	if (yu_arm_gpio_lock_param.io)
		goto exit;

	res = yu_arm_gpio_lock_param.res;
	size = resource_size(res);

	if (!devm_request_mem_region(dev, res->start, size, res->name)) {
		ret = -EFAULT;
		goto exit;
	}

	yu_arm_gpio_lock_param.io = devm_ioremap(dev, res->start, size);
	if (!yu_arm_gpio_lock_param.io)
		ret = -ENOMEM;

exit:
	mutex_unlock(yu_arm_gpio_lock_param.lock);

	return ret;
}

/* Request memory region and map yu cause_gpio_arm.coalesce0 resource */
static int mlxbf2_gpio_get_yu_cause_gpio_res(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	resource_size_t size;
	int ret = 0;

	mutex_lock(yu_cause_gpio_param.lock);

	/* Check if the memory map already exists */
	if (yu_cause_gpio_param.io)
		goto exit;

	res = yu_cause_gpio_param.res;
	size = resource_size(res);

	if (!devm_request_mem_region(dev, res->start, size, res->name)) {
		ret = -EFAULT;
		goto exit;
	}

	yu_cause_gpio_param.io = devm_ioremap(dev, res->start, size);
	if (!yu_cause_gpio_param.io)
		ret = -ENOMEM;

exit:
	mutex_unlock(yu_cause_gpio_param.lock);

	return ret;
}

/*
 * Acquire the YU arm_gpio_lock to be able to change the direction
 * mode. If the lock_active bit is already set, return an error.
 */
static int mlxbf2_gpio_lock_acquire(struct mlxbf2_gpio_context *gs)
{
	u32 arm_gpio_lock_val;

	mutex_lock(yu_arm_gpio_lock_param.lock);
	spin_lock(&gs->gc.bgpio_lock);

	arm_gpio_lock_val = readl(yu_arm_gpio_lock_param.io);

	/*
	 * When lock active bit[31] is set, ModeX is write enabled
	 */
	if (YU_LOCK_ACTIVE_BIT(arm_gpio_lock_val)) {
		spin_unlock(&gs->gc.bgpio_lock);
		mutex_unlock(yu_arm_gpio_lock_param.lock);
		return -EINVAL;
	}

	writel(YU_ARM_GPIO_LOCK_ACQUIRE, yu_arm_gpio_lock_param.io);

	return 0;
}

/*
 * Release the YU arm_gpio_lock after changing the direction mode.
 */
static void mlxbf2_gpio_lock_release(struct mlxbf2_gpio_context *gs)
	__releases(&gs->gc.bgpio_lock)
	__releases(yu_arm_gpio_lock_param.lock)
{
	writel(YU_ARM_GPIO_LOCK_RELEASE, yu_arm_gpio_lock_param.io);
	spin_unlock(&gs->gc.bgpio_lock);
	mutex_unlock(yu_arm_gpio_lock_param.lock);
}

/*
 * mode0 and mode1 are both locked by the gpio_lock field.
 *
 * Together, mode0 and mode1 define the gpio Mode dependeing also
 * on Reg_DataOut.
 *
 * {mode1,mode0}:{Reg_DataOut=0,Reg_DataOut=1}->{DataOut=0,DataOut=1}
 *
 * {0,0}:Reg_DataOut{0,1}->{Z,Z} Input PAD
 * {0,1}:Reg_DataOut{0,1}->{0,1} Full drive Output PAD
 * {1,0}:Reg_DataOut{0,1}->{0,Z} 0-set PAD to low, 1-float
 * {1,1}:Reg_DataOut{0,1}->{Z,1} 0-float, 1-set PAD to high
 */

/*
 * Set input direction:
 * {mode1,mode0} = {0,0}
 */
static int mlxbf2_gpio_direction_input(struct gpio_chip *chip,
				       unsigned int offset)
{
	struct mlxbf2_gpio_context *gs = gpiochip_get_data(chip);
	int ret;

	/*
	 * Although the arm_gpio_lock was set in the probe function, check again
	 * if it is still enabled to be able to write to the ModeX registers.
	 */
	ret = mlxbf2_gpio_lock_acquire(gs);
	if (ret < 0)
		return ret;

	writel(BIT(offset), gs->gpio_io + YU_GPIO_MODE0_CLEAR);
	writel(BIT(offset), gs->gpio_io + YU_GPIO_MODE1_CLEAR);

	mlxbf2_gpio_lock_release(gs);

	return ret;
}

/*
 * Set output direction:
 * {mode1,mode0} = {0,1}
 */
static int mlxbf2_gpio_direction_output(struct gpio_chip *chip,
					unsigned int offset,
					int value)
{
	struct mlxbf2_gpio_context *gs = gpiochip_get_data(chip);
	int ret = 0;
	u32 val;

	/*
	 * Although the arm_gpio_lock was set in the probe function,
	 * check again it is still enabled to be able to write to the
	 * ModeX registers.
	 */
	ret = mlxbf2_gpio_lock_acquire(gs);
	if (ret < 0)
		return ret;

	writel(BIT(offset), gs->gpio_io + YU_GPIO_MODE1_CLEAR);
	writel(BIT(offset), gs->gpio_io + YU_GPIO_MODE0_SET);

	/*
	 * Set {functional_enable1,functional_enable0}={0,0}
	 * to give control to software over these GPIOs.
	 */
	val = readl(gs->gpio_io + YU_GPIO_FUNCTIONAL_ENABLE1);
	val &= ~BIT(offset);
	writel(val, gs->gpio_io + YU_GPIO_FUNCTIONAL_ENABLE1);
	val = readl(gs->gpio_io + YU_GPIO_FUNCTIONAL_ENABLE0);
	val &= ~BIT(offset);
	writel(val, gs->gpio_io + YU_GPIO_FUNCTIONAL_ENABLE0);

	mlxbf2_gpio_lock_release(gs);

	return ret;
}

static void mlxbf2_gpio_send_work(struct work_struct *work)
{
#ifdef  CONFIG_ACPI
	acpi_bus_generate_netlink_event("button/power.*", "Power Button",
					0x80, 1);
#endif
}

static u32 mlxbf2_gpio_get_int_mask(struct mlxbf2_gpio_context *gs)
{
	u32 gpio_int_mask = 0;

	/*
	 * Determine bit mask within the yu gpio block.
	 */
	if (gs->phy_int_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK)
		gpio_int_mask = BIT(gs->phy_int_pin);
	if (gs->rst_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK)
		gpio_int_mask |= BIT(gs->rst_pin);
	if (gs->low_pwr_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK)
		gpio_int_mask = BIT(gs->low_pwr_pin);

	return gpio_int_mask;
}

static bool mlxbf2_gpio_is_acpi_event(u32 gpio_block, unsigned long gpio_pin,
			  struct mlxbf2_gpio_context *gs)
{
	if (gpio_block & BIT(GPIO_BLOCK0)) {
		if (gpio_pin & BIT(gs->rst_pin))
			return true;
	}
	if (gpio_block & BIT(GPIO_BLOCK16)) {
		if (gpio_pin & BIT(gs->low_pwr_pin))
			return true;
	}

	return false;
}

static irqreturn_t mlxbf2_gpio_irq_handler(int irq, void *ptr)
{
	struct mlxbf2_gpio_context *gs = ptr;
	unsigned long gpio_pin;
	u32 gpio_block, val;
	unsigned long flags;

	spin_lock_irqsave(&gs->gc.bgpio_lock, flags);

	/*
	 * Determine which yu gpio block this interrupt is for.
	 * Return if the interrupt is not for gpio block 0 or
	 * gpio block 16.
	 */
	gpio_block = readl(yu_cause_gpio_param.io);
	if (!(gpio_block & BIT(GPIO_BLOCK0)) &&
	    !(gpio_block & BIT(GPIO_BLOCK16))) {
		spin_unlock_irqrestore(&gs->gc.bgpio_lock, flags);
		return IRQ_NONE;
	}

	/*
	 * Check if the interrupt signaled by this yu gpio block is supported.
	 */
	gpio_pin = readl(gs->gpio_io + YU_GPIO_CAUSE_OR_CAUSE_EVTEN0);
	if (!(gpio_pin & gs->gpio_int_mask)) {
		spin_unlock_irqrestore(&gs->gc.bgpio_lock, flags);
		return IRQ_NONE;
	}

	/*
	 * Clear interrupt when done, otherwise, no further interrupt
	 * will be triggered.
	 */
	val = readl(gs->gpio_io + YU_GPIO_CAUSE_OR_CLRCAUSE);
	val |= gpio_pin;
	writel(val, gs->gpio_io + YU_GPIO_CAUSE_OR_CLRCAUSE);

	if ((gpio_block & BIT(GPIO_BLOCK0)) && (gpio_pin & BIT(gs->phy_int_pin)))
		generic_handle_irq(irq_find_mapping(gs->gc.irq.domain, gs->phy_int_pin));

	spin_unlock_irqrestore(&gs->gc.bgpio_lock, flags);

	if (mlxbf2_gpio_is_acpi_event(gpio_block, gpio_pin, gs))
		schedule_work(&gs->send_work);

	return IRQ_HANDLED;
}

static void mlxbf2_gpio_irq_unmask(struct irq_data *data)
{
}

static void mlxbf2_gpio_irq_mask(struct irq_data *data)
{
}

static int mlxbf2_gpio_init_hw(struct gpio_chip *gc)
{
	struct mlxbf2_gpio_context *gs = gpiochip_get_data(gc);
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&gs->gc.bgpio_lock, flags);

	/* Clear all interrupts */
	val = readl(gs->gpio_io + YU_GPIO_CAUSE_OR_CLRCAUSE);
	val |= gs->gpio_int_mask;
	writel(val, gs->gpio_io + YU_GPIO_CAUSE_OR_CLRCAUSE);

	if (gs->low_pwr_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK) {
		val = readl(gs->gpio_io + YU_GPIO_CAUSE_RISE_EN);
		val |= gs->gpio_int_mask;
		writel(val, gs->gpio_io + YU_GPIO_CAUSE_RISE_EN);
	}

	val = readl(gs->gpio_io + YU_GPIO_CAUSE_FALL_EN);
	val |= gs->gpio_int_mask;
	writel(val, gs->gpio_io + YU_GPIO_CAUSE_FALL_EN);

	/*
	 * Setting the priority for the GPIO interrupt enables the
	 * interrupt as well
	 */
	val = readl(gs->gpio_io + YU_GPIO_CAUSE_OR_EVTEN0);
	val |= gs->gpio_int_mask;
	writel(val, gs->gpio_io + YU_GPIO_CAUSE_OR_EVTEN0);

	spin_unlock_irqrestore(&gs->gc.bgpio_lock, flags);

	return 0;
}

static void mlxbf2_gpio_disable_int(struct mlxbf2_gpio_context *gs)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&gs->gc.bgpio_lock, flags);
	val = readl(gs->gpio_io + YU_GPIO_CAUSE_OR_EVTEN0);
	val &= ~gs->gpio_int_mask;
	writel(val, gs->gpio_io + YU_GPIO_CAUSE_OR_EVTEN0);
	spin_unlock_irqrestore(&gs->gc.bgpio_lock, flags);
}

static int mlxbf2_gpio_to_irq(struct gpio_chip *chip, unsigned gpio)
{
	struct mlxbf2_gpio_context *gs;

	gs = gpiochip_get_data(chip);

	return irq_create_mapping(gs->gc.irq.domain, gpio);
}

/* BlueField-2 GPIO driver initialization routine. */
static int
mlxbf2_gpio_probe(struct platform_device *pdev)
{
	struct mlxbf2_gpio_context *gs;
	struct device *dev = &pdev->dev;
	struct gpio_irq_chip *girq;
	unsigned int low_pwr_pin;
	unsigned int phy_int_pin;
	unsigned int rst_pin;
	struct gpio_chip *gc;
	struct resource *res;
	unsigned int npins;
	const char *name;
	int ret, irq;

	name = dev_name(dev);

	gs = devm_kzalloc(dev, sizeof(*gs), GFP_KERNEL);
	if (!gs)
		return -ENOMEM;

	spin_lock_init(&gs->gc.bgpio_lock);
	INIT_WORK(&gs->send_work, mlxbf2_gpio_send_work);

	/* YU GPIO block address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	gs->gpio_io = devm_ioremap(dev, res->start, resource_size(res));
	if (!gs->gpio_io)
		return -ENOMEM;

	ret = mlxbf2_gpio_get_lock_res(pdev);
	if (ret) {
		dev_err(dev, "Failed to get yu_arm_gpio_lock resource\n");
		return ret;
	}

	ret = mlxbf2_gpio_get_yu_cause_gpio_res(pdev);
	if (ret) {
		dev_err(dev, "Failed to get yu cause_gpio_arm.coalesce0 resource\n");
		return ret;
	}

	if (device_property_read_u32(dev, "npins", &npins))
		npins = MLXBF2_GPIO_MAX_PINS_PER_BLOCK;

	gc = &gs->gc;

	ret = bgpio_init(gc, dev, 4,
			gs->gpio_io + YU_GPIO_DATAIN,
			gs->gpio_io + YU_GPIO_DATASET,
			gs->gpio_io + YU_GPIO_DATACLEAR,
			NULL,
			NULL,
			0);

	if (ret) {
		dev_err(dev, "bgpio_init failed\n");
		return ret;
	}

	gc->direction_input = mlxbf2_gpio_direction_input;
	gc->direction_output = mlxbf2_gpio_direction_output;
	gc->ngpio = npins;
	gc->owner = THIS_MODULE;
	gc->to_irq = mlxbf2_gpio_to_irq;

	/*
	 * PHY interrupt
	 */
	ret = device_property_read_u32(dev, "phy-int-pin", &phy_int_pin);
	if (ret < 0)
		phy_int_pin = MLXBF2_GPIO_MAX_PINS_PER_BLOCK;

	/*
	 * OCP3.0 supports the low power mode interrupt.
	 */
	ret = device_property_read_u32(dev, "low-pwr-pin", &low_pwr_pin);
	if (ret < 0)
		low_pwr_pin = MLXBF2_GPIO_MAX_PINS_PER_BLOCK;

	/*
	 * BlueSphere and the PRIS boards support the reset interrupt.
	 */
	ret = device_property_read_u32(dev, "rst-pin", &rst_pin);
	if (ret < 0)
		rst_pin = MLXBF2_GPIO_MAX_PINS_PER_BLOCK;

	gs->phy_int_pin = phy_int_pin;
	gs->low_pwr_pin = low_pwr_pin;
	gs->rst_pin = rst_pin;
	gs->gpio_int_mask = mlxbf2_gpio_get_int_mask(gs);

	if (gs->gpio_int_mask) {
		gs->irq_chip.name = name;
		gs->irq_chip.irq_mask = mlxbf2_gpio_irq_mask;
		gs->irq_chip.irq_unmask = mlxbf2_gpio_irq_unmask;

		girq = &gs->gc.irq;
		girq->chip = &gs->irq_chip;
		/* This will let us handle the parent IRQ in the driver */
		girq->parent_handler = NULL;
		girq->num_parents = 0;
		girq->parents = NULL;
		girq->default_type = IRQ_TYPE_NONE;
		girq->handler = handle_simple_irq;
		girq->init_hw = mlxbf2_gpio_init_hw;

		irq = platform_get_irq(pdev, 0);
		ret = devm_request_irq(dev, irq, mlxbf2_gpio_irq_handler,
				       IRQF_ONESHOT | IRQF_SHARED, name, gs);
		if (ret) {
			dev_err(dev, "failed to request IRQ");
			return ret;
		}
	}

	ret = devm_gpiochip_add_data(dev, &gs->gc, gs);
	if (ret) {
		dev_err(dev, "Failed adding memory mapped gpiochip\n");
		return ret;
	}
	platform_set_drvdata(pdev, gs);

	if (phy_int_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK) {
		/* Create phy irq mapping */
		mlxbf2_gpio_to_irq(&gs->gc, phy_int_pin);
		/* Enable sharing the irq domain with the PHY driver */
		irq_set_default_host(gs->gc.irq.domain);
	}

	return 0;
}

static int
mlxbf2_gpio_remove(struct platform_device *pdev)
{
	struct mlxbf2_gpio_context *gs;

	gs = platform_get_drvdata(pdev);

	if ((gs->phy_int_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK) ||
	    (gs->low_pwr_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK) ||
	    (gs->rst_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK)) {
		mlxbf2_gpio_disable_int(gs);
	}

	if ((gs->low_pwr_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK) ||
	    (gs->rst_pin != MLXBF2_GPIO_MAX_PINS_PER_BLOCK)) {
		flush_work(&gs->send_work);
	}

	return 0;
}

#ifdef CONFIG_PM
static int mlxbf2_gpio_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct mlxbf2_gpio_context *gs = platform_get_drvdata(pdev);

	gs->csave_regs->gpio_mode0 = readl(gs->gpio_io +
		YU_GPIO_MODE0);
	gs->csave_regs->gpio_mode1 = readl(gs->gpio_io +
		YU_GPIO_MODE1);

	return 0;
}

static int mlxbf2_gpio_resume(struct platform_device *pdev)
{
	struct mlxbf2_gpio_context *gs = platform_get_drvdata(pdev);

	writel(gs->csave_regs->gpio_mode0, gs->gpio_io +
		YU_GPIO_MODE0);
	writel(gs->csave_regs->gpio_mode1, gs->gpio_io +
		YU_GPIO_MODE1);

	return 0;
}
#endif

static const struct acpi_device_id __maybe_unused mlxbf2_gpio_acpi_match[] = {
	{ "MLNXBF22", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, mlxbf2_gpio_acpi_match);

static struct platform_driver mlxbf2_gpio_driver = {
	.driver = {
		.name = "mlxbf2_gpio",
		.acpi_match_table = ACPI_PTR(mlxbf2_gpio_acpi_match),
	},
	.probe    = mlxbf2_gpio_probe,
	.remove   = mlxbf2_gpio_remove,
#ifdef CONFIG_PM
	.suspend  = mlxbf2_gpio_suspend,
	.resume   = mlxbf2_gpio_resume,
#endif
};

module_platform_driver(mlxbf2_gpio_driver);

MODULE_DESCRIPTION("Mellanox BlueField-2 GPIO Driver");
MODULE_AUTHOR("Asmaa Mnebhi <asmaa@nvidia.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);
