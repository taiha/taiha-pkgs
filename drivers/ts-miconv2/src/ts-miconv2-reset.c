// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"ts-miconv2-reset"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>

#include "ts-miconv2.h"

#define MICON_CMD_PWR_OFF	0x6
#define MICON_CMD_SHTDN_WAIT	0xc
#define MICON_CMD_SHTDN_CANCEL	0xd
#define MICON_CMD_REBOOT	0xe
#define MICON_CMD_WOL_RDY_ON	0x29
#define MICON_CMD_WOL_RDY_OFF	0x2a
#define MICON_CMD_PWR_STATE	0x46

struct micon_reset {
	struct miconv2 *micon;
	struct notifier_block nb;
};

static struct micon_reset *miconv2_rst;

static int ts_miconv2_reset_common(u8 pwr_stat)
{
	int ret;

	/* shutdown wait */
	ret = ts_miconv2_write_flag(miconv2_rst->micon, MICON_CMD_SHTDN_WAIT);
	if (ret)
		return ret;
	/* power state */
	return ts_miconv2_write_u8(miconv2_rst->micon, MICON_CMD_PWR_STATE,
				   pwr_stat);
}

static int ts_miconv2_reset(struct notifier_block *nb,
			     unsigned long mode, void *cmd)
{
	int ret;

	ret = ts_miconv2_reset_common(MICON_PWR_STAT_RESTART);
	if (!ret)
		ret = ts_miconv2_write_flag(miconv2_rst->micon, MICON_CMD_REBOOT);
	if (ret)
		pr_err("failed to reset (%d)\n", ret);

	return NOTIFY_DONE;
}

static void ts_miconv2_poweroff(void)
{
	int ret;

	ret = ts_miconv2_reset_common(MICON_PWR_STAT_PWROFF);
	if (!ret)
		ret = ts_miconv2_write_flag(miconv2_rst->micon, MICON_CMD_PWR_OFF);
	if (ret)
		pr_err("failed to power off (%d)\n", ret);
}

static int ts_miconv2_reset_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	pr_info("MICON v2 Reset driver\n");

	miconv2_rst = devm_kzalloc(dev, sizeof(*miconv2_rst), GFP_KERNEL);
	if (!miconv2_rst) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	miconv2_rst->micon = dev_get_drvdata(dev->parent);
	if (!miconv2_rst->micon)
		return -EINVAL;
	miconv2_rst->nb.notifier_call = ts_miconv2_reset;
	/*
	 * use the higher priority than the
	 * restart handler of the architecture
	 */
	miconv2_rst->nb.priority = 140;

	dev_set_drvdata(dev, miconv2_rst);

	/* set power_state=3(running) */
	ret = ts_miconv2_write_u8(miconv2_rst->micon, MICON_CMD_PWR_STATE,
				  MICON_PWR_STAT_RUNNING);
	if (ret) {
		pr_info("failed to set power_state to MICON\n");
		return ret;
	}

	ret = register_restart_handler(&miconv2_rst->nb);
	if (ret) {
		dev_err(dev, "failed to register MICON v2 reset\n");
		return -ENODEV;
	}

	pm_power_off = ts_miconv2_poweroff;

	return ret;
}

static const struct of_device_id ts_miconv2_reset_ids[] = {
	{ .compatible = "buffalo,ts-miconv2-reset" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ts_miconv2_reset_ids);

static struct platform_driver ts_miconv2_reset_driver = {
	.probe = ts_miconv2_reset_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ts_miconv2_reset_ids,
	},
};
module_platform_driver(ts_miconv2_reset_driver);

MODULE_DESCRIPTION("MICON v2 Restart driver for Buffalo TeraStation series");
MODULE_LICENSE("GPL v2");
