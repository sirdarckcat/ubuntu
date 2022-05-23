/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("LICENSE"). Unless the LICENSE provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written
 * permission.
 *
 * This software and the related documents are provided as is, with no
 * express or implied warranties, other than those that are expressly
 * stated in the License.
 *
 */
#ifndef __BACKPORT_LINUX_PHY_H
#define __BACKPORT_LINUX_PHY_H
#include_next <linux/phy.h>
#include <linux/compiler.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,9,0)
#define phy_connect(dev, bus_id, handler, interface) \
	phy_connect(dev, bus_id, handler, 0, interface)
#endif

#if LINUX_VERSION_IS_LESS(4,5,0)
#define phydev_name LINUX_I915_BACKPORT(phydev_name)
static inline const char *phydev_name(const struct phy_device *phydev)
{
	return dev_name(&phydev->dev);
}

#define mdiobus_is_registered_device LINUX_I915_BACKPORT(mdiobus_is_registered_device)
static inline bool mdiobus_is_registered_device(struct mii_bus *bus, int addr)
{
	return bus->phy_map[addr];
}

#define phy_attached_print LINUX_I915_BACKPORT(phy_attached_print)
void phy_attached_print(struct phy_device *phydev, const char *fmt, ...)
	__printf(2, 3);
#define phy_attached_info LINUX_I915_BACKPORT(phy_attached_info)
void phy_attached_info(struct phy_device *phydev);

static inline int backport_mdiobus_register(struct mii_bus *bus)
{
	bus->irq = kmalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	if (!bus->irq) {
		pr_err("mii_bus irq allocation failed\n");
		return -ENOMEM;
	}

	memset(bus->irq, PHY_POLL, sizeof(int) * PHY_MAX_ADDR);

/* in kernel 4.3 a #define for mdiobus_register is added to the kernel. */
#ifndef mdiobus_register
	return mdiobus_register(bus);
#else
	return __mdiobus_register(bus, THIS_MODULE);
#endif
}
#ifdef mdiobus_register
#undef mdiobus_register
#endif
#define mdiobus_register LINUX_I915_BACKPORT(mdiobus_register)

static inline void backport_mdiobus_unregister(struct mii_bus *bus)
{
	kfree(bus->irq);
	mdiobus_unregister(bus);
}
#define mdiobus_unregister LINUX_I915_BACKPORT(mdiobus_unregister)
#endif /* < 4.5 */

#if LINUX_VERSION_IS_LESS(4,5,0)
#define phydev_get_addr LINUX_I915_BACKPORT(phydev_get_addr)
static inline int phydev_get_addr(struct phy_device *phydev)
{
	return phydev->addr;
}
#else
#define phydev_get_addr LINUX_I915_BACKPORT(phydev_get_addr)
static inline int phydev_get_addr(struct phy_device *phydev)
{
	return phydev->mdio.addr;
}
#endif

#endif /* __BACKPORT_LINUX_PHY_H */
