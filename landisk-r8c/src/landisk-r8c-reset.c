// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"landisk-r8c-reset"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>
#include <linux/phy.h>

#include "landisk-r8c.h"

#define CMD_RESET	"reset"
#define CMD_POFF	"poweroff"
#define CMD_WOL		"wol_flag"
#define CMD_PWR_AC	"intrp"
#define CMD_DUMMY	"sts"

#define MII_MARVELL_LED_PAGE		3
#define MII_MARVELL_WOL_PAGE		17
#define MII_MARVELL_PHY_PAGE		22

#define MII_PHY_LED_POL_CTRL		17
#define MII_88E1318_PHY_LED_TCR		18
#define MII_88E1318_PHY_WOL_CTRL	16

#define MII_88E1318_PHY_LED_POL_CTRL_LED01		BIT(0) | BIT(2)
#define MII_88E1318_PHY_LED_TCR_INTn_ENABLE		BIT(7)
#define MII_88E1318_PHY_WOL_CTRL_CLEAR_WOL_STATUS	BIT(12)

struct r8c_reset {
	struct r8c_mcu *r8c;
	struct notifier_block nb;
	struct phy_device *phydev;
};

static struct r8c_reset *r8c_rst;

static int landisk_r8c_reset(struct notifier_block *nb,
			     unsigned long mode, void *cmd)
{
	char buf[4];
	int ret;

	/* work-around for reset command failure */
	ret = landisk_r8c_exec_cmd(r8c_rst->r8c, CMD_DUMMY, NULL, buf, 4);
	if (ret <= 0) {
		pr_err("r8c-reset: failed to execute dummy command %d\n",
		       ret);
		return NOTIFY_DONE;
	}

	ret = landisk_r8c_exec_cmd(r8c_rst->r8c, CMD_RESET, NULL, NULL, 0);
	if (ret)
		pr_err("failed to execute reset %d\n", ret);

	return NOTIFY_DONE;
}

static void landisk_r8c_poweroff(void)
{
	int ret = 0, saved_page;

	saved_page = phy_select_page(r8c_rst->phydev, MII_MARVELL_LED_PAGE);
	if (saved_page < 0)
		goto out;

	/* Set the INTn pin to the required state */
	__phy_modify(r8c_rst->phydev, MII_88E1318_PHY_LED_TCR,
		     MII_88E1318_PHY_LED_TCR_INTn_ENABLE,
		     MII_88E1318_PHY_LED_TCR_INTn_ENABLE);

	/* Set the LED[0],[1] polarity bits to the required state */
	__phy_modify(r8c_rst->phydev, MII_PHY_LED_POL_CTRL,
		     MII_88E1318_PHY_LED_POL_CTRL_LED01,
		     MII_88E1318_PHY_LED_POL_CTRL_LED01);

	/*
	 * If WOL was enabled and a magic packet was received before powering
	 * off, we won't be able to wake up by sending another magic packet.
	 * Clear WOL status.
	 */
	__phy_write(r8c_rst->phydev, MII_MARVELL_PHY_PAGE, MII_MARVELL_WOL_PAGE);
	__phy_set_bits(r8c_rst->phydev, MII_88E1318_PHY_WOL_CTRL,
		       MII_88E1318_PHY_WOL_CTRL_CLEAR_WOL_STATUS);

out:
	ret = phy_restore_page(r8c_rst->phydev, saved_page, ret);
	if (!ret)
		ret = landisk_r8c_exec_cmd(r8c_rst->r8c, CMD_POFF, NULL, NULL, 0);
	if (ret)
		pr_err("failed to execute poweroff %d\n", ret);
}

static ssize_t landisk_r8c_flag_set(const char *buf, size_t len, const char *cmd,
				    const char *arg_en, const char *arg_dis)
{
	ssize_t ret;
	char cmdbuf[4];
	u32 enable;

	ret = kstrtou32(buf, 0, &enable);
	if (ret)
		return ret;

	ret = landisk_r8c_exec_cmd(r8c_rst->r8c, cmd,
				   enable ? arg_en : arg_dis,
				   cmdbuf, 4);
	/*
	 * R8C returns result of execution
	 * response length is not 1 (err included), or not success('0')
	 */
	if (ret != 1 || cmdbuf[0] != '0')
		return ret < 0 ? ret : -EINVAL;

	return len;
}

static ssize_t wol_flag_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	ssize_t ret;
	char cmdbuf[4];

	ret = landisk_r8c_exec_cmd(r8c_rst->r8c, CMD_WOL, NULL, cmdbuf, 4);
	if (ret <= 0)
		return 0;

	return scnprintf(buf, 4, "%s\n", cmdbuf);
}

static ssize_t wol_flag_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t len)
{
	return landisk_r8c_flag_set(buf, len, CMD_WOL, "set", "remove");
}

static DEVICE_ATTR_RW(wol_flag);

static ssize_t power_on_ac_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	char cmdbuf[4];

	ret = landisk_r8c_exec_cmd(r8c_rst->r8c, CMD_PWR_AC, "3", cmdbuf, 4);
	if (ret <= 0)
		return 0;

	return scnprintf(buf, 4, "%s\n", cmdbuf);
}

static ssize_t power_on_ac_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	return landisk_r8c_flag_set(buf, len, CMD_PWR_AC, "3 1", "3 0");
}

static DEVICE_ATTR_RW(power_on_ac);

static struct attribute *r8c_poweroff_attrs[] = {
	&dev_attr_wol_flag.attr,
	&dev_attr_power_on_ac.attr,
	NULL,
};

static const struct attribute_group r8c_poweroff_attr_group = {
	.attrs = r8c_poweroff_attrs,
};

static int landisk_r8c_reset_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mii_bus *bus;
	struct device_node *dn;
	int ret;

	pr_info("R8C reset driver\n");

	r8c_rst = devm_kzalloc(dev, sizeof(*r8c_rst), GFP_KERNEL);
	if (!r8c_rst) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	r8c_rst->r8c = dev_get_drvdata(dev->parent);
	if (!r8c_rst->r8c) {
		dev_err(dev, "failed to get r8c struct\n");
		return -EINVAL;
	}
	r8c_rst->nb.notifier_call = landisk_r8c_reset;
	/*
	 * use the higher priority than the
	 * restart handler of the architecture
	 */
	r8c_rst->nb.priority = 140;

	dev_set_drvdata(dev, r8c_rst);

	ret = register_restart_handler(&r8c_rst->nb);
	if (ret) {
		dev_err(dev, "failed to register R8C reset\n");
		return -ENODEV;
	}

	dn = of_find_node_by_name(NULL, "mdio-bus");
	if (!dn)
		return -ENODEV;

	bus = of_mdio_find_bus(dn);
	of_node_put(dn);
	if (!bus)
		return -EPROBE_DEFER;

	r8c_rst->phydev = phy_find_first(bus);
	put_device(&bus->dev);
	if (!r8c_rst->phydev)
		return -EPROBE_DEFER;
	dev_info(dev, "got phy device (addr: %d)\n", r8c_rst->phydev->mdio.addr);

	pm_power_off = landisk_r8c_poweroff;

	ret = sysfs_create_group(&dev->kobj, &r8c_poweroff_attr_group);

	return ret;
}

static const struct of_device_id landisk_r8c_reset_ids[] = {
	{ .compatible = "iodata,landisk-r8c-reset" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, landisk_r8c_reset_ids);

static struct platform_driver landisk_r8c_reset_driver = {
	.probe = landisk_r8c_reset_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = landisk_r8c_reset_ids,
	},
};
module_platform_driver(landisk_r8c_reset_driver);

MODULE_DESCRIPTION("R8C Restart driver for I-O DATA LAN DISK series");
MODULE_LICENSE("GPL v2");
