/*******************************************************************************
 * Copyright © 2015-2016, Shuge
 *		Author: Sugar <shugeLinux@gmail.com>
 *
 * This file is provided under a dual BSD/GPL license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 ********************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/irq.h>

#include <linux/mfd/acx00-mfd.h>

#define EXTEPHY_CTRL0 0x0014
#define EXTEPHY_CTRL1 0x0016

#define EPHY_CTRL 0x6000

struct ephy_res {
	struct device_driver *plat_drv;
	struct device_driver *phy_drv;
	struct acx00 *acx;
};
static struct ephy_res ephy_priv;

static int ephy_reset(struct phy_device *phydev)
{
	int bmcr;

	/* Software Reset PHY */
	bmcr = phy_read(phydev, MII_BMCR);
	if (bmcr < 0)
		return bmcr;
	bmcr |= BMCR_RESET;
	bmcr = phy_write(phydev, MII_BMCR, bmcr);
	if (bmcr < 0)
		return bmcr;

	do {
		bmcr = phy_read(phydev, MII_BMCR);
		if (bmcr < 0)
			return bmcr;
	} while (bmcr & BMCR_RESET);

	return 0;
}

static int ephy_config_init(struct phy_device *phydev)
{
	int c;

	c = ephy_reset(phydev);
	if (c < 0)
		return c;

	/* Iint ephy */
	phy_write(phydev, 0x1f, 0x0100);	/* Switch to Page 1 */
	phy_write(phydev, 0x12, 0x4824);	/* Disable APS */

	phy_write(phydev, 0x1f, 0x0200);	/* Switch to Page 2 */
	phy_write(phydev, 0x18, 0x0000);	/* PHYAFE TRX optimization */

	phy_write(phydev, 0x1f, 0x0600);	/* Switch to Page 6 */
	phy_write(phydev, 0x14, 0x708f);	/* PHYAFE TX optimization */
	phy_write(phydev, 0x13, 0xF000);	/* PHYAFE RX optimization */
	phy_write(phydev, 0x15, 0x1530);

	phy_write(phydev, 0x1f, 0x0800);	/* Switch to Page 6 */
	phy_write(phydev, 0x18, 0x00bc);	/* PHYAFE TRX optimization */

	/* Disable Auto Power Saving mode */
	phy_write(phydev, 0x1f, 0x0100);	/* Switch to Page 1 */
	c = phy_read(phydev, 0x17);
	c &= ~BIT(13);
	phy_write(phydev, 0x17, c);
	return phy_write(phydev, 0x1f, 0x0000);	/* Switch to Page 0 */

}

static int ephy_probe(struct phy_device *phydev)
{
	struct phy_driver *drv;

	if (!phydev)
		return -ENODEV;

	drv = phydev->drv;
	ephy_priv.phy_drv = &drv->driver;

	return 0;
}

#if 0
static int ephy_ack_interrupt(struct phy_device *phydev)
{
	int err = phy_read(phydev, IP101A_G_IRQ_CONF_STATUS);
	if (err < 0)
		return err;

	return 0;
}
#endif

static struct phy_driver ephy_driver = {
	.phy_id		= 0x00441400,
	.name		= "ephy",
	.phy_id_mask	= 0x0ffffff0,
	.features	= PHY_BASIC_FEATURES | SUPPORTED_Pause |
			  SUPPORTED_Asym_Pause,
#if 0
	.flags		= PHY_HAS_INTERRUPT,
	.ack_interrupt	= ephy_ack_interrupt,
#endif
	.config_init	= &ephy_config_init,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver		= { .owner = THIS_MODULE,},
	.probe		= ephy_probe,
};

static const struct platform_device_id sunxi_ephy_id[] = {
	{ "acx-ephy", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, sunxi_ephy_id);

static __devinit int ephy_plat_probe(struct platform_device *pdev)
{
	struct acx00 *ax = dev_get_drvdata(pdev->dev.parent);
	int value;

	if (!ax)
		return -ENODEV;

	ephy_priv.acx = ax;
	platform_set_drvdata(pdev, &ephy_priv);
	ephy_priv.plat_drv = pdev->dev.driver;

#ifdef CONFIG_MFD_AC200
	value = acx00_reg_read(ax, EXTEPHY_CTRL0);
	value |= 0x03;
	acx00_reg_write(ax, EXTEPHY_CTRL0, value);
	value = acx00_reg_read(ax, EXTEPHY_CTRL1);
	value |= 0x0f;
	acx00_reg_write(ax, EXTEPHY_CTRL1, value);
	acx00_reg_write(ax, EPHY_CTRL, 0x06);
#endif

	return 0;
}

static int __devexit ephy_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver ephy_plat_driver = {
	.driver = {
		.name = "acx-ephy",
		.owner = THIS_MODULE,
	},
	.probe = ephy_plat_probe,
	.remove = __devexit_p(ephy_plat_remove),
	.id_table = sunxi_ephy_id,
};

static int __init ephy_init(void)
{
	int ret = 0;

	platform_driver_register(&ephy_plat_driver);
	ret = phy_driver_register(&ephy_driver);

	return ret;
}

static void __exit ephy_exit(void)
{
	if (ephy_priv.plat_drv)
		platform_driver_unregister(&ephy_plat_driver);

	phy_driver_unregister(&ephy_driver);
}

module_init(ephy_init);
module_exit(ephy_exit);

static struct mdio_device_id __maybe_unused ephy_tbl[] = {
	{ 0x00441400, 0x0ffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, ephy_tbl);

MODULE_DESCRIPTION("Allwinner EPHY drivers");
MODULE_AUTHOR("Sugar <shugeLinux@gmail.com>");
MODULE_LICENSE("GPL");
