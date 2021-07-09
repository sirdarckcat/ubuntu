// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause

/* MDIO support for Mellanox Gigabit Ethernet driver
 *
 * Copyright (c) 2020 NVIDIA Corporation.
 */

#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/irqreturn.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include "mlxbf_gige.h"

#define MLXBF_GIGE_MDIO_GW_OFFSET	0x0
#define MLXBF_GIGE_MDIO_CFG_OFFSET	0x4

/* Support clause 22 */
#define MLXBF_GIGE_MDIO_CL22_ST1	0x1
#define MLXBF_GIGE_MDIO_CL22_WRITE	0x1
#define MLXBF_GIGE_MDIO_CL22_READ	0x2

/* Busy bit is set by software and cleared by hardware */
#define MLXBF_GIGE_MDIO_SET_BUSY	0x1

/* MDIO GW register bits */
#define MLXBF_GIGE_MDIO_GW_AD_MASK	GENMASK(15, 0)
#define MLXBF_GIGE_MDIO_GW_DEVAD_MASK	GENMASK(20, 16)
#define MLXBF_GIGE_MDIO_GW_PARTAD_MASK	GENMASK(25, 21)
#define MLXBF_GIGE_MDIO_GW_OPCODE_MASK	GENMASK(27, 26)
#define MLXBF_GIGE_MDIO_GW_ST1_MASK	GENMASK(28, 28)
#define MLXBF_GIGE_MDIO_GW_BUSY_MASK	GENMASK(30, 30)

/* MDIO config register bits */
#define MLXBF_GIGE_MDIO_CFG_MDIO_MODE_MASK		GENMASK(1, 0)
#define MLXBF_GIGE_MDIO_CFG_MDIO3_3_MASK		GENMASK(2, 2)
#define MLXBF_GIGE_MDIO_CFG_MDIO_FULL_DRIVE_MASK	GENMASK(4, 4)
#define MLXBF_GIGE_MDIO_CFG_MDC_PERIOD_MASK		GENMASK(15, 8)
#define MLXBF_GIGE_MDIO_CFG_MDIO_IN_SAMP_MASK		GENMASK(23, 16)
#define MLXBF_GIGE_MDIO_CFG_MDIO_OUT_SAMP_MASK		GENMASK(31, 24)

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

#define MLXBF_GIGE_MDIO_PERIOD	(((MLXBF_GIGE_MDC_CLK_NS * MLXBF_GIGE_I1CLK_MHZ / 1000) / 2) - 1)

#define MLXBF_GIGE_MDIO_CFG_VAL (FIELD_PREP(MLXBF_GIGE_MDIO_CFG_MDIO_MODE_MASK, 1) | \
				 FIELD_PREP(MLXBF_GIGE_MDIO_CFG_MDIO3_3_MASK, 1) | \
				 FIELD_PREP(MLXBF_GIGE_MDIO_CFG_MDIO_FULL_DRIVE_MASK, 1) | \
				 FIELD_PREP(MLXBF_GIGE_MDIO_CFG_MDC_PERIOD_MASK, \
					    MLXBF_GIGE_MDIO_PERIOD) |   \
				 FIELD_PREP(MLXBF_GIGE_MDIO_CFG_MDIO_IN_SAMP_MASK, 6) | \
				 FIELD_PREP(MLXBF_GIGE_MDIO_CFG_MDIO_OUT_SAMP_MASK, 13))

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

	gw_reg |= FIELD_PREP(MLXBF_GIGE_MDIO_GW_AD_MASK, data);
	gw_reg |= FIELD_PREP(MLXBF_GIGE_MDIO_GW_DEVAD_MASK, phy_reg);
	gw_reg |= FIELD_PREP(MLXBF_GIGE_MDIO_GW_PARTAD_MASK, phy_add);
	gw_reg |= FIELD_PREP(MLXBF_GIGE_MDIO_GW_OPCODE_MASK, opcode);
	gw_reg |= FIELD_PREP(MLXBF_GIGE_MDIO_GW_ST1_MASK,
			     MLXBF_GIGE_MDIO_CL22_ST1);
	gw_reg |= FIELD_PREP(MLXBF_GIGE_MDIO_GW_BUSY_MASK,
			     MLXBF_GIGE_MDIO_SET_BUSY);

	return gw_reg;
}

static int mlxbf_gige_mdio_read(struct mii_bus *bus, int phy_add, int phy_reg)
{
	struct mlxbf_gige *priv = bus->priv;
	u32 cmd;
	int ret;
	u32 val;

	if (phy_reg & MII_ADDR_C45)
		return -EOPNOTSUPP;

	/* Send mdio read request */
	cmd = mlxbf_gige_mdio_create_cmd(0, phy_add, phy_reg, MLXBF_GIGE_MDIO_CL22_READ);

	writel(cmd, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);

	ret = readl_poll_timeout_atomic(priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET,
					val, !(val & MLXBF_GIGE_MDIO_GW_BUSY_MASK), 100, 1000000);

	if (ret) {
		writel(0, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);
		return ret;
	}

	ret = readl(priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);
	/* Only return ad bits of the gw register */
	ret &= MLXBF_GIGE_MDIO_GW_AD_MASK;

	return ret;
}

static int mlxbf_gige_mdio_write(struct mii_bus *bus, int phy_add,
				 int phy_reg, u16 val)
{
	struct mlxbf_gige *priv = bus->priv;
	u32 cmd;
	int ret;
	u32 temp;

	if (phy_reg & MII_ADDR_C45)
		return -EOPNOTSUPP;

	/* Send mdio write request */
	cmd = mlxbf_gige_mdio_create_cmd(val, phy_add, phy_reg,
					 MLXBF_GIGE_MDIO_CL22_WRITE);
	writel(cmd, priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET);

	/* If the poll timed out, drop the request */
	ret = readl_poll_timeout_atomic(priv->mdio_io + MLXBF_GIGE_MDIO_GW_OFFSET,
					temp, !(temp & MLXBF_GIGE_MDIO_GW_BUSY_MASK), 100, 1000000);

	return ret;
}

static void mlxbf_gige_mdio_disable_phy_int(struct mlxbf_gige *priv)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&priv->gpio_lock, flags);
	val = readl(priv->gpio_io + MLXBF_GIGE_GPIO_CAUSE_OR_EVTEN0);
	val &= ~MLXBF_GIGE_CAUSE_OR_EVTEN0_MASK;
	writel(val, priv->gpio_io + MLXBF_GIGE_GPIO_CAUSE_OR_EVTEN0);
	spin_unlock_irqrestore(&priv->gpio_lock, flags);
}

static void mlxbf_gige_mdio_enable_phy_int(struct mlxbf_gige *priv)
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

	/* Enable PHY interrupt by setting the priority level */
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
irqreturn_t mlxbf_gige_mdio_handle_phy_interrupt(int irq, void *dev_id)
{
	struct phy_device *phydev;
	struct mlxbf_gige *priv;
	u32 val;

	priv = dev_id;
	phydev = priv->netdev->phydev;

	/* Check if this interrupt is from PHY device.
	 * Return if it is not.
	 */
	val = readl(priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_CAUSE_EVTEN0);
	if (!(val & MLXBF_GIGE_CAUSE_OR_CAUSE_EVTEN0_MASK))
		return IRQ_NONE;

	phy_mac_interrupt(phydev);

	/* Clear interrupt when done, otherwise, no further interrupt
	 * will be triggered.
	 */
	val = readl(priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_CLRCAUSE);
	val |= MLXBF_GIGE_CAUSE_OR_CLRCAUSE_MASK;
	writel(val, priv->gpio_io +
			MLXBF_GIGE_GPIO_CAUSE_OR_CLRCAUSE);

	/* Make sure to clear the PHY device interrupt */
	if (phydev->drv->ack_interrupt)
		phydev->drv->ack_interrupt(phydev);

	phydev->interrupts = PHY_INTERRUPT_ENABLED;
	if (phydev->drv->config_intr)
		phydev->drv->config_intr(phydev);

	return IRQ_HANDLED;
}

int mlxbf_gige_mdio_probe(struct platform_device *pdev, struct mlxbf_gige *priv)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
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

	/* Configure mdio parameters */
	writel(MLXBF_GIGE_MDIO_CFG_VAL,
	       priv->mdio_io + MLXBF_GIGE_MDIO_CFG_OFFSET);

	mlxbf_gige_mdio_enable_phy_int(priv);

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

	ret = mdiobus_register(priv->mdiobus);
	if (ret)
		dev_err(dev, "Failed to register MDIO bus\n");

	return ret;
}

void mlxbf_gige_mdio_remove(struct mlxbf_gige *priv)
{
	mlxbf_gige_mdio_disable_phy_int(priv);
	mdiobus_unregister(priv->mdiobus);
}
