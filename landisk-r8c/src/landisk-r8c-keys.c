// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"landisk-r8c-keys"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/input.h>

#include "landisk-r8c.h"

#define R8C_KEYS		2

#define R8C_POWER		'P'
#define R8C_RESET		'R'

struct r8c_key_device {
	char evcode;
	unsigned int lkcode;
	char *name;
};

struct r8c_keys {
	struct notifier_block nb;
	char last_code;
	struct input_dev *keys[R8C_KEYS];
};

/*
 * - power -> Power ON or Power Off
 * - reset -> System Reset
 *
 * Note: HDL-XR/XV series also has "FUNC" key (evcode='c'/'C')
 */
static const struct r8c_key_device key_list[] = {
	{
		.evcode = R8C_POWER,
		.lkcode = KEY_POWER,
		.name = "power",
	},
	{
		.evcode = R8C_RESET,
		.lkcode = KEY_RESTART,
		.name = "reset",
	},
};

static int r8c_key_event(struct notifier_block *nb,
			 unsigned long evcode, void *data)
{
	struct r8c_keys *k = container_of(nb, struct r8c_keys, nb);
	struct input_dev *idev;
	char code = (char)evcode;
	int i;
	bool is_lower = code >= 'a' ? true : false;

	for (i = 0; i < R8C_KEYS; i++) {
		if (code != key_list[i].evcode + (is_lower ? 0x20 : 0x0))
			continue;

		/*
		 * check duplicates
		 *   skip if the same with the last code
		 *
		 * R8C returns the same code if the key was holden
		 * while the specific time
		 * ex.:
		 *   'R' (pressed), 'r' (released), 'R' (pressed),
		 *   -> 'R' (held), 'r' (released)
		 */
		if (k->last_code == code)
			return NOTIFY_STOP;
		k->last_code = code;

		idev = k->keys[i];

		/*
		 * lower: released
		 * upper: pressed
		 */
		input_report_key(idev, key_list[i].lkcode, is_lower ? 0 : 1);
		input_sync(idev);

		dev_dbg(&idev->dev, "%s %s\n",
			idev->name, is_lower ? "released" : "pressed");

		return NOTIFY_STOP;
	}

	return NOTIFY_DONE;
}

static int landisk_r8c_keys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct r8c_keys *k;
	struct input_dev *idev;
	int i, ret;

	pr_info("R8C key driver\n");

	k = devm_kzalloc(dev, sizeof(*k), GFP_KERNEL);
	if (!k)
		return -ENOMEM;

	for (i = 0; i < R8C_KEYS; i++) {
		idev = devm_input_allocate_device(dev);
		if (!idev)
			return -ENOMEM;

		idev->name = key_list[i].name;
		input_set_capability(idev, EV_KEY, key_list[i].lkcode);

		ret = input_register_device(idev);
		if (ret)
			return ret;

		k->keys[i] = idev;
	}

	k->last_code = 0x0;
	k->nb.notifier_call = r8c_key_event;
	k->nb.priority = 128;

	return devm_landisk_r8c_register_event_notifier(dev, &k->nb);
}

static const struct of_device_id landisk_r8c_keys_ids[] = {
	{ .compatible = "iodata,landisk-r8c-keys" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, landisk_r8c_keys_ids);

static struct platform_driver landisk_r8c_keys_driver = {
	.probe = landisk_r8c_keys_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = landisk_r8c_keys_ids,
	},
};
module_platform_driver(landisk_r8c_keys_driver);

MODULE_DESCRIPTION("R8C Key driver for I-O DATA LAN DISK series");
MODULE_LICENSE("GPL v2");
