// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"ts-miconv2-wdt"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/watchdog.h>

#include "ts-miconv2.h"

#define MICON_CMD_SYSTEM_WDT	0x35

/* timeout defs */
#define MICON_WDT_TO_MAX	254
#define MICON_WDT_TO_MIN	1
#define MICON_WDT_TO_DIS	0
#define MICON_WDT_TO_DEF	240	/* 4 min. */

#define MICON_WDT_HEARTBEAT_MS	20000	/* 20 sec. */

struct micon_wdt {
	struct miconv2 *micon;
	struct watchdog_device wdev;
};

static int ts_miconv2_wdt_set_timeout(struct watchdog_device *wdev,
				      unsigned int timeout)
{
	struct micon_wdt *w = watchdog_get_drvdata(wdev);

	dev_dbg(wdev->parent, "set timeout=%u\n", timeout);
	if (timeout < 0 || timeout > MICON_WDT_TO_MAX)
		return -EINVAL;
	/* <set val> = 0xff - timeout */
	if (timeout != MICON_WDT_TO_DIS)
		timeout = 255 - timeout;
	return ts_miconv2_write_u8(w->micon, MICON_CMD_SYSTEM_WDT, (u8)timeout);
}

static int ts_miconv2_wdt_ping(struct watchdog_device *wdev)
{
	dev_dbg(wdev->parent, "ping watchdog\n");
	return ts_miconv2_wdt_set_timeout(wdev, MICON_WDT_TO_DEF);
}

static int ts_miconv2_wdt_stop(struct watchdog_device *wdev)
{
	int ret;
	ret = ts_miconv2_wdt_set_timeout(wdev, MICON_WDT_TO_DIS);
	if (!ret)
		dev_info(wdev->parent, "watchdog stopped\n");
	return ret;
}

static int ts_miconv2_wdt_start(struct watchdog_device *wdev)
{
	int ret;
	ret = ts_miconv2_wdt_set_timeout(wdev, MICON_WDT_TO_DEF);
	if (!ret)
		dev_info(wdev->parent, "watchdog started (timeout=%u)\n",
			 MICON_WDT_TO_DEF);
	return ret;
}

static ssize_t system_wdt_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct micon_wdt *w = dev_get_drvdata(dev);
	ssize_t ret;
	u8 val;

	ret = ts_miconv2_read_u8(w->micon, MICON_CMD_SYSTEM_WDT, &val, true);
	if (ret)
		return ret;
	return scnprintf(buf, 8, "%u\n", val);
}
static DEVICE_ATTR_RO(system_wdt);

static const struct watchdog_info ts_miconv2_wdt_info = {
	.identity = DRV_NAME,
	.options  = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING,
};

static const struct watchdog_ops ts_miconv2_wdt_ops = {
	.owner = THIS_MODULE,
	.start = ts_miconv2_wdt_start,
	.stop  = ts_miconv2_wdt_stop,
	.ping  = ts_miconv2_wdt_ping,
	.set_timeout = ts_miconv2_wdt_set_timeout,
};

static int ts_miconv2_wdt_probe(struct platform_device *pdev)
{
	struct micon_wdt *w;
	struct device *dev = &pdev->dev;
	int ret;

	pr_info("MICON v2 WDT driver\n");

	w = devm_kzalloc(dev, sizeof(*w), GFP_KERNEL);
	if (!w) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	w->micon = dev_get_drvdata(dev->parent);
	if (!w->micon)
		return -EINVAL;
	dev_set_drvdata(dev, w);

	w->wdev.parent = dev;
	w->wdev.info = &ts_miconv2_wdt_info;
	w->wdev.ops = &ts_miconv2_wdt_ops;
	w->wdev.max_timeout = MICON_WDT_TO_MAX;
	w->wdev.min_timeout = MICON_WDT_TO_MIN;
	w->wdev.timeout = MICON_WDT_TO_DEF;
	w->wdev.min_hw_heartbeat_ms = MICON_WDT_HEARTBEAT_MS;

	watchdog_set_drvdata(&w->wdev, w);

	ts_miconv2_wdt_stop(&w->wdev);

	ret = devm_watchdog_register_device(dev, &w->wdev);
	if (ret)
		return ret;

	dev_info(dev, "watchdog registered (timeout=%d sec, nowayout=%d)\n",
		 w->wdev.timeout,
		 test_bit(WDOG_NO_WAY_OUT, &w->wdev.status) ? 1 : 0);

	device_create_file(dev, &dev_attr_system_wdt);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id ts_miconv2_wdt_ids[] = {
	{ .compatible = "buffalo,ts-miconv2-reset" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ts_miconv2_wdt_ids);

static struct platform_driver ts_miconv2_wdt_driver = {
	.probe = ts_miconv2_wdt_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ts_miconv2_wdt_ids,
	},
};
module_platform_driver(ts_miconv2_wdt_driver);

MODULE_DESCRIPTION("MICON v2 Restart driver for Buffalo TeraStation series");
MODULE_LICENSE("GPL v2");
