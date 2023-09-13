// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"landisk-r8c-beeper"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/input.h>
#include <linux/workqueue.h>

#include "landisk-r8c.h"

#define CMD_MML		"mml"

struct r8c_beeper {
	struct r8c_mcu *r8c;

	bool on;
	struct work_struct work;
};

static int landisk_r8c_beeper_set(struct r8c_beeper *b, char *mml)
{
	return landisk_r8c_exec_cmd(b->r8c, CMD_MML, mml, NULL, 0);
}

static void landisk_r8c_beeper_work(struct work_struct *work)
{
	struct r8c_beeper *b = container_of(work, struct r8c_beeper, work);

	landisk_r8c_beeper_set(b, b->on ? "|;0v1q8o6c4r8;|" : NULL);
}

static int landisk_r8c_beeper_event(struct input_dev *idev, unsigned int type,
				    unsigned int code, int value)
{
	struct r8c_beeper *b = input_get_drvdata(idev);

	pr_info("event called, type=%u, code=%u, value=%d\n", type, code, value);
	if (type != EV_SND || code != SND_BELL)
		return -ENOTSUPP;

	if (value < 0)
		return -EINVAL;

	b->on = value;
	/* Schedule work to actually turn the beeper on or off */
	schedule_work(&b->work);

	return 0;
}

static void landisk_r8c_beeper_close(struct input_dev *idev)
{
	struct r8c_beeper *b = input_get_drvdata(idev);

	cancel_work_sync(&b->work);
	landisk_r8c_beeper_set(b, NULL);
}

static int landisk_r8c_beeper_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct r8c_beeper *b;
	struct input_dev *idev;

	pr_info("R8C beeper driver\n");

	b = devm_kzalloc(dev, sizeof(*b), GFP_KERNEL);
	if (!b)
		return -ENOMEM;

	b->r8c = dev_get_drvdata(dev->parent);
	if (!b->r8c) {
		dev_err(dev, "failed to get r8c struct\n");
		return -EINVAL;
	}

	idev = devm_input_allocate_device(&pdev->dev);
	if (!idev)
		return -ENOMEM;

	INIT_WORK(&b->work, landisk_r8c_beeper_work);

	idev->name		= pdev->name;
	idev->id.bustype	= BUS_HOST;
	idev->id.vendor		= 0x0001;
	idev->id.product	= 0x0001;
	idev->id.version	= 0x0100;
	idev->close		= landisk_r8c_beeper_close;
	idev->event		= landisk_r8c_beeper_event;

	input_set_capability(idev, EV_SND, SND_BELL);
	input_set_drvdata(idev, b);

	return input_register_device(idev);
}

static const struct of_device_id landisk_r8c_beeper_ids[] = {
	{ .compatible = "iodata,landisk-r8c-beeper" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, landisk_r8c_beeper_ids);

static struct platform_driver landisk_r8c_beeper_driver = {
	.probe = landisk_r8c_beeper_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = landisk_r8c_beeper_ids,
	},
};
module_platform_driver(landisk_r8c_beeper_driver);

MODULE_DESCRIPTION("R8C Beeper driver for I-O DATA LAN DISK series");
MODULE_LICENSE("GPL v2");
