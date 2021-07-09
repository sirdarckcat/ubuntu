// SPDX-License-Identifier: GPL-2.0-only OR Linux-OpenIB
/*  MDIO support for Mellanox GigE driver
 *
 *  Copyright (C) 2020 Mellanox Technologies, Ltd.
 */

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irqreturn.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include "mlxbf_gige.h"

#define MLXBF_GIGE_POLL_BUSY_TIMEOUT	100 /* ms */
#define MLXBF_GIGE_POLL_DELAY_USEC	100 /* microsec */

#define MLXBF_GIGE_MDIO_GW_OFFSET	0x0
#define MLXBF_GIGE_MDIO_CFG_OFFSET	0x4

/* Support clause 22 */
#define MLXBF_GIGE_CL22_ST1	        0x1
#define MLXBF_GIGE_CL22_MDIO_WRITE	0x1
#define MLXBF_GIGE_CL22_MDIO_READ	0x2

/* Busy bit is set by software and cleared by hardware */
#define MLXBF_GIGE_SET_MDIO_BUSY	0x1
/* Lock bit should be set/cleared by software */
#define MLXBF_GIGE_SET_MDIO_LOCK	0x1

/* MDIO GW register bits */
#define MLXBF_GIGE_MDIO_GW_AD_SHIFT		0
#define MLXBF_GIGE_MDIO_GW_AD_MASK		GENMASK(15, 0)
#define MLXBF_GIGE_MDIO_GW_DEVAD_SHIFT		16
#define MLXBF_GIGE_MDIO_GW_DEVAD_MASK		GENMASK(20, 16)
#define MLXBF_GIGE_MDIO_GW_PARTAD_SHIFT		21
#define MLXBF_GIGE_MDIO_GW_PARTAD_MASK		GENMASK(25, 21)
#define MLXBF_GIGE_MDIO_GW_OPCODE_SHIFT		26
#define MLXBF_GIGE_MDIO_GW_OPCODE_MASK		GENMASK(27, 26)
#define MLXBF_GIGE_MDIO_GW_ST1_SHIFT		28
#define MLXBF_GIGE_MDIO_GW_ST1_MASK		GENMASK(28, 28)
#define MLXBF_GIGE_MDIO_GW_BUSY_SHIFT		30
#define MLXBF_GIGE_MDIO_GW_BUSY_MASK		GENMASK(30, 30)
#define MLXBF_GIGE_MDIO_GW_LOCK_SHIFT		31
#define MLXBF_GIGE_MDIO_GW_LOCK_MASK		GENMASK(31, 31)

/* MDIO config register bits */
#define MLXBF_GIGE_MDIO_CFG_MDIO_MODE_SHIFT		0
#define MLXBF_GIGE_MDIO_CFG_MDIO3_3_SHIFT		2
#define MLXBF_GIGE_MDIO_CFG_MDIO_FULL_DRIVE_SHIFT	4
#define MLXBF_GIGE_MDIO_CFG_MDC_PERIOD_SHIFT		8
#define MLXBF_GIGE_MDIO_CFG_MASTER_IN_SAMP_SHIFT	16
#define MLXBF_GIGE_MDIO_CFG_MDIO_OUT_SAMP_SHIFT		24

/* Formula for encoding the MDIO period. The encoded value is
 * passed to the MDIO config register.
 *
 * mdc_clk = 2*(val + 1)*i1clk
 *
 * 400 ns = 2*(val + 1)*(((1/430)*1000) ns)
 *
 * val = (((400 * 430 / 1000) / 2) - 1)
 */
#define MLXBF_GIGE_I1CLK_MHZ		430
#define MLXBF_GIGE_MDC_CLK_NS		400

#define MLXBF_GIGE_MDIO_PERIOD_VAL	(((MLXBF_GIGE_MDC_CLK_NS * MLXBF_GIGE_I1CLK_MHZ / 1000) / 2) - 1)
#define MLXBF_GIGE_MDIO_PERIOD		(MLXBF_GIGE_MDIO_PERIOD_VAL << MLXBF_GIGE_MDIO_CFG_MDC_PERIOD_SHIFT)

/* PHY should operate in master mode only */
#define MLXBF_GIGE_MDIO_MODE_MASTER	(0x1 << MLXBF_GIGE_MDIO_CFG_MDIO_MODE_SHIFT)
/* PHY input voltage has to be 3.3V */
#define MLXBF_GIGE_MDIO3_3		(0x1 << MLXBF_GIGE_MDIO_CFG_MDIO3_3_SHIFT)
/* Operate in full drive mode */
#define MLXBF_GIGE_MDIO_FULL_DRIVE	(0x1 << MLXBF_GIGE_MDIO_CFG_MDIO_FULL_DRIVE_SHIFT)
/* 6 cycles before the i1clk (core clock) rising edge that triggers the mdc */
#define	MLXBF_GIGE_MDIO_MASTER_IN_SAMP	(6 << MLXBF_GIGE_MDIO_CFG_MASTER_IN_SAMP_SHIFT)
/* 13 cycles after the i1clk (core clock) rising edge that triggers the mdc */
#define MLXBF_GIGE_MDIO_MDIO_OUT_SAMP	(13 << MLXBF_GIGE_MDIO_CFG_MDIO_OUT_SAMP_SHIFT)

#define MLXBF_GIGE_MDIO_CFG_VAL (MLXBF_GIGE_MDIO_MODE_MASTER | \
				 MLXBF_GIGE_MDIO3_3 | \
				 MLXBF_GIGE_MDIO_FULL_DRIVE | \
				 MLXBF_GIGE_MDIO_PERIOD | \
				 MLXBF_GIGE_MDIO_MASTER_IN_SAMP | \
				 MLXBF_GIGE_MDIO_MDIO_OUT_SAMP)

/* The PHY interrupt line is shared with other interrupt lines such
 * as GPIO and SMBus. So use YU registers to determine whether the
 * interrupt comes from the PHY.
 */
#define MLXBF_GIGE_CAUSE_RSH_COALESCE0_GPIO_CAUSE_MASK	0x10
#define MLXBF_GIGE_GPIO_CAUSE_IRQ_IS_SET(val) \
	((val) & MLXBF_GIGE_CAUSE_RSH_COALESCE0_GPIO_CAUSE_MASK)

#define MLXBF_GIGE_GPIO_BLOCK0_MASK	BIT(0)

#define MLXBF_GIGE_GPIO_CAUSE_FALL_EN		0x48
#define MLXBF_GIGE_GPIO_CAUSE_OR_CAUSE_EVTEN0	0x80
#define MLXBF_GIGE_GPIO_CAUSE_OR_EVTEN0		0x94
#define MLXBF_GIGE_GPIO_CAUSE_OR_CLRCAUSE	0x98

#define MLXBF_GIGE_GPIO12_BIT			12
#define MLXBF_GIGE_CAUSE_OR_CAUSE_EVTEN0_MASK	BIT(MLXBF_GIGE_GPIO12_BIT)
#define MLXBF_GIGE_CAUSE_OR_EVTEN0_MASK		BIT(MLXBF_GIGE_GPIO12_BIT)
#define MLXBF_GIGE_CAUSE_FALL_EN_MASK		BIT(MLXBF_GIGE_GPIO12_BIT)
#define MLXBF_GIGE_CAUSE_OR_CLRCAUSE_MASK	BIT(MLXBF_GIGE_GPIO12_BIT)

static u32 mlxbf_gige_mdio_create_cmd(u16 data, int phy_add,
				      int phy_reg, u32 opcode)
{
	u32 gw_reg = 0;

	gw_reg |= (data << MLXBF_GIGE_MDIO_GW_AD_SHIFT) &
			MLXBF_GIGE_MDIO_GW_AD_MASK;
	gw_reg |= ((phy_reg << MLXBF_GIGE_MDIO_GW_DEVAD_SHIFT) &
			MLXBF_GIGE_MDIO_GW_DEVAD_MASK);
	gw_reg |= ((phy_add << MLXBF_GIGE_MDIO_GW_PARTAD_SHIFT) &
			MLXBF_GIGE_MDIO_GW_PARTAD_MASK);
	gw_reg |= ((opcode << MLXBF_GIGE_MDIO_GW_OPCODE_SHIFT) &
			MLXBF_GIGE_MDIO_GW_OPCODE_MASK);
	gw_reg |= ((MLXBF_GIGE_CL22_ST1 << MLXBF_GIGE_MDIO_GW_ST1_SHIFT) &
			MLXBF_GIGE_MDIO_GW_ST1_MASK);
	gw_reg |= ((MLXBF_GIGE_SET_MDIO_BUSY << MLXBF_GIGE_MDIO_GW_BUSY_SHIFT) &
			MLXBF_GIGE_MDIO_GW_BUSY_MASK);

	/* Hold the lock until the read/write is completed so that no other
	 * program accesses the mdio bus.
	 */
	gw_reg |= ((MLXBF_GIGE_SET_MDIO_LOCK << MLXBF_GIGE_MDIO_GW_LOCK_SHIFT) &
			MLXBF_GIGE_MDIO_GW_LOCK_MASK);

	return gw_reg;
}

static int mlxbf_gige_mdio_poll_bit(struct mlxbf_gige *priv, u32 bit_mask)
{
	unsigned long timeout;
	u32 val;

	timeout = jiffies + msecs_to_jiffies(MLXBF_GIGE_POLL_BUSY_TIMEOUT);
	do {
		val = readl(priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);
		if (!(val & bit_mask))
			return 0;
		udelay(MLXBF_GIGE_POLL_DELAY_USEC);
	} while (time_before(jiffies, timeout));

	return -ETIME;
}

static int mlxbf_gige_mdio_read(struct mii_bus *bus, int phy_add, int phy_reg)
{
	struct mlxbf_gige *priv = bus->priv;
	u32 cmd;
	u32 ret;

	/* If the lock is held by something else, drop the request.
	 * If the lock is cleared, that means the busy bit was cleared.
	 */
	ret = mlxbf_gige_mdio_poll_bit(priv, MLXBF_GIGE_MDIO_GW_LOCK_MASK);
	if (ret)
		return -EBUSY;

	/* Send mdio read request */
	cmd = mlxbf_gige_mdio_create_cmd(0, phy_add, phy_reg, MLXBF_GIGE_CL22_MDIO_READ);

	writel(cmd, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);

	ret = mlxbf_gige_mdio_poll_bit(priv, MLXBF_GIGE_MDIO_GW_BUSY_MASK);
	if (ret) {
		writel(0, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);
		return -EBUSY;
	}

	ret = readl(priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);
	/* Only return ad bits of the gw register */
	ret &= MLXBF_GIGE_MDIO_GW_AD_MASK;

	/* To release the YU MDIO lock, clear gw register,
	 * so that the YU does not confuse this write with a new
	 * MDIO read/write request.
	 */
	writel(0, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);

	return ret;
}

static int mlxbf_gige_mdio_write(struct mii_bus *bus, int phy_add,
				 int phy_reg, u16 val)
{
	struct mlxbf_gige *priv = bus->priv;
	u32 cmd;
	int ret;

	/* If the lock is held by something else, drop the request.
	 * If the lock is cleared, that means the busy bit was cleared.
	 */
	ret = mlxbf_gige_mdio_poll_bit(priv, MLXBF_GIGE_MDIO_GW_LOCK_MASK);
	if (ret)
		return -EBUSY;

	/* Send mdio write request */
	cmd = mlxbf_gige_mdio_create_cmd(val, phy_add, phy_reg,
					 MLXBF_GIGE_CL22_MDIO_WRITE);
	writel(cmd, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);

	/* If the poll timed out, drop the request */
	ret = mlxbf_gige_mdio_poll_bit(priv, MLXBF_GIGE_MDIO_GW_BUSY_MASK);

	/* To release the YU MDIO lock, clear gw register,
	 * so that the YU does not confuse this write as a new
	 * MDIO read/write request.
	 */
	writel(0, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);

	return ret;
}

static void mlxbf_gige_mdio_disable_gpio12_irq(struct mlxbf_gige *priv)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&priv->gpio_lock, flags);
	val = readl(priv->gpio_io + MLXBF_GIGE_GPIO_CAUSE_OR_EVTEN0);
	val &= ~MLXBF_GIGE_CAUSE_OR_EVTEN0_MASK;
	writel(val, priv->gpio_io + MLXBF_GIGE_GPIO_CAUSE_OR_EVTEN0);
	spin_unlock_irqrestore(&priv->gpio_lock, flags);
}

static void mlxbf_gige_mdio_enable_gpio12_irq(struct mlxbf_gige *priv)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&priv->gpio_lock, flags);
	/* The INT_N interrupt level is active low.
	 * So enable cause fall bit to detect when GPIO
	 * state goes low.
	 */
	val = readl(priv->gpio_io + MLXBF_GIGE_GPIO_CAUSE_FALL_EN);
	val |= MLXBF_GIGE_CAUSE_FALL_EN_MASK;
	writel(val, priv->gpio_io + MLXBF_GIGE_GPIO_CAUSE_FALL_EN);

	/* Enable GPIO 12 interrupt by setting the priority level */
	val = readl(priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_EVTEN0);
	val |= MLXBF_GIGE_CAUSE_OR_EVTEN0_MASK;
	writel(val, priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_EVTEN0);
	spin_unlock_irqrestore(&priv->gpio_lock, flags);
}

/* Interrupt handler is called from mlxbf_gige_main.c
 * driver whenever a phy interrupt is received.
 */
irqreturn_t mlxbf_gige_mdio_handle_phy_interrupt(struct mlxbf_gige *priv)
{
	u32 val;

	/* The YU interrupt is shared between SMBus and GPIOs.
	 * So first, determine whether this is a GPIO interrupt.
	 */
	val = readl(priv->cause_rsh_coalesce0_io);
	if (!MLXBF_GIGE_GPIO_CAUSE_IRQ_IS_SET(val)) {
		/* Nothing to do here, not a GPIO interrupt */
		return IRQ_NONE;
	}
	/* Then determine which gpio register this interrupt is for.
	 * Return if the interrupt is not for gpio block 0.
	 */
	val = readl(priv->cause_gpio_arm_coalesce0_io);
	if (!(val & MLXBF_GIGE_GPIO_BLOCK0_MASK))
		return IRQ_NONE;

	/* Finally check if this interrupt is for gpio pin 12.
	 * Return if it is not.
	 */
	val = readl(priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_CAUSE_EVTEN0);
	if (!(val & MLXBF_GIGE_CAUSE_OR_CAUSE_EVTEN0_MASK))
		return IRQ_NONE;

	/* Clear interrupt when done, otherwise, no further interrupt
	 * will be triggered.
	 * Writing 0x1 to the clrcause register also clears the
	 * following registers:
	 * cause_gpio_arm_coalesce0
	 * cause_rsh_coalesce0
	 */
	val = readl(priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_CLRCAUSE);
	val |= MLXBF_GIGE_CAUSE_OR_CLRCAUSE_MASK;
	writel(val, priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_CLRCAUSE);

	return IRQ_HANDLED;
}

int mlxbf_gige_mdio_probe(struct platform_device *pdev, struct mlxbf_gige *priv)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	u32 phy_addr;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_MDIO9);
	if (!res)
		return -ENODEV;

	priv->mdio_io = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->mdio_io))
		return PTR_ERR(priv->mdio_io);

	res = platform_get_resource(pdev, IORESOURCE_MEM, MLXBF_GIGE_RES_GPIO0);
	if (!res)
		return -ENODEV;

	priv->gpio_io = devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->gpio_io)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM,
				    MLXBF_GIGE_RES_CAUSE_RSH_COALESCE0);
	if (!res)
		return -ENODEV;

	priv->cause_rsh_coalesce0_io =
		devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->cause_rsh_coalesce0_io)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM,
				    MLXBF_GIGE_RES_CAUSE_GPIO_ARM_COALESCE0);
	if (!res)
		return -ENODEV;

	priv->cause_gpio_arm_coalesce0_io =
		devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->cause_gpio_arm_coalesce0_io)
		return -ENOMEM;

	/* Configure mdio parameters */
	writel(MLXBF_GIGE_MDIO_CFG_VAL,
	       priv->mdio_io + MLXBF_GIGE_MDIO_CFG_OFFSET);

	mlxbf_gige_mdio_enable_gpio12_irq(priv);

	priv->mdiobus = devm_mdiobus_alloc(dev);
	if (!priv->mdiobus) {
		dev_err(dev, "Failed to alloc MDIO bus\n");
		return -ENOMEM;
	}

	priv->mdiobus->name = "mlxbf-mdio";
	priv->mdiobus->read = mlxbf_gige_mdio_read;
	priv->mdiobus->write = mlxbf_gige_mdio_write;
	priv->mdiobus->parent = dev;
	priv->mdiobus->priv = priv;
	snprintf(priv->mdiobus->id, MII_BUS_ID_SIZE, "%s",
		 dev_name(dev));

	ret = device_property_read_u32(dev, "phy-addr", &phy_addr);
	if (ret < 0)
		phy_addr = MLXBF_GIGE_DEFAULT_PHY_ADDR;

	priv->mdiobus->irq[phy_addr] = PHY_POLL;

	/* Auto probe PHY at the corresponding address */
	priv->mdiobus->phy_mask = ~(1 << phy_addr);
	ret = mdiobus_register(priv->mdiobus);
	if (ret)
		dev_err(dev, "Failed to register MDIO bus\n");

	return ret;
}

void mlxbf_gige_mdio_remove(struct mlxbf_gige *priv)
{
	mlxbf_gige_mdio_disable_gpio12_irq(priv);
	mdiobus_unregister(priv->mdiobus);
}
