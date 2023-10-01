// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"landisk-r8c-keys"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/input.h>

#include "landisk-r8c.h"

#define R8C_KEYS		2

#define R8C_EV_POWER		'P'
#define R8C_EV_RESET		'R'
#define R8C_EV_FUNC		'C'
#define R8C_EV_HDD1		'W'
#define R8C_EV_HDD2		'X'
#define R8C_EV_HDD3		'Y'
#define R8C_EV_HDD4		'Z'

struct r8c_key_device {
	char *name;
	char evcode;
	unsigned int lkcode;
	unsigned short models;
};

struct r8c_keys {
	struct input_dev *idev;
	struct notifier_block nb;
	u32 id;
	char last_code;
};

/*
 * - power -> Power ON or Power Off
 * - reset -> System Reset
 * - func  -> Function
 * - hddN  -> HDD Detection
 */
static const struct r8c_key_device r8c_key_list[] = {
	{
		.name   = "power",
		.evcode = R8C_EV_POWER,
		.lkcode = KEY_POWER,
		.models = BIT(ID_HDL_A) | BIT(ID_HDL2_A) | BIT(ID_HDL_XR),
	},
	{
		.name   = "reset",
		.evcode = R8C_EV_RESET,
		.lkcode = KEY_RESTART,
		.models = BIT(ID_HDL_A) | BIT(ID_HDL2_A) | BIT(ID_HDL_XR),
	},
	{
		.name   = "func",
		.evcode = R8C_EV_FUNC,
		.lkcode = BTN_0,
		.models = BIT(ID_HDL_XR),
	},
	{
		.name   = "hdd1",
		.evcode = R8C_EV_HDD1,
		.lkcode = BTN_1,
		.models = BIT(ID_HDL_XR),
	},
	{
		.name   = "hdd2",
		.evcode = R8C_EV_HDD2,
		.lkcode = BTN_2,
		.models = BIT(ID_HDL_XR),
	},
	{
		.name   = "hdd3",
		.evcode = R8C_EV_HDD3,
		.lkcode = BTN_3,
		.models = BIT(ID_HDL_XR),
	},
	{
		.name   = "hdd4",
		.evcode = R8C_EV_HDD4,
		.lkcode = BTN_4,
		.models = BIT(ID_HDL_XR),
	},
};

static int r8c_key_event(struct notifier_block *nb,
			 unsigned long evcode, void *data)
{
	struct r8c_keys *k = container_of(nb, struct r8c_keys, nb);
	const struct r8c_key_device *kdev;
	char code = (char)evcode;
	int i;
	bool is_lower = code >= 'a' ? true : false;

	for (i = 0; i < ARRAY_SIZE(r8c_key_list); i++) {
		kdev = &r8c_key_list[i];

		if (code != kdev->evcode + (is_lower ? 0x20 : 0x0))
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

		/*
		 * lower: released
		 * upper: pressed
		 */
		input_report_key(k->idev, kdev->lkcode, is_lower ? 0 : 1);
		input_sync(k->idev);

		dev_dbg(&k->idev->dev, "%s %s\n",
			kdev->name, is_lower ? "released" : "pressed");

		return NOTIFY_STOP;
	}

	return NOTIFY_DONE;
}

static int landisk_r8c_keys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct r8c_keys *k;
	struct input_dev *idev;
	unsigned short *keymap;
	u32 id = *(u32 *)dev_get_platdata(dev);
	int i, ret;

	pr_info("R8C key driver\n");

	k = devm_kzalloc(dev, sizeof(*k), GFP_KERNEL);
	if (!k)
		return -ENOMEM;

	keymap = devm_kcalloc(dev, ARRAY_SIZE(r8c_key_list),
			      sizeof(r8c_key_list[0].lkcode), GFP_KERNEL);
	if (!keymap)
		return -ENOMEM;

	idev = devm_input_allocate_device(dev);
	if (!idev)
		return -ENOMEM;

	platform_set_drvdata(pdev, k);
	input_set_drvdata(idev, k);

	for (i = 0; i < ARRAY_SIZE(r8c_key_list); i++) {
		/* skip unsupported button */
		if (!(r8c_key_list[i].models & BIT(id)))
			continue;
		keymap[i] = r8c_key_list[i].lkcode;
		input_set_capability(idev, EV_KEY, keymap[i]);
		dev_dbg(dev, "setup \"%s\" button\n", r8c_key_list[i].name);
	}

	idev->name = pdev->name;
	idev->phys = "landisk-r8c-keys/input0";
	idev->dev.parent = dev;
	idev->id.bustype = BUS_HOST;
	idev->id.vendor = 0x0001;
	idev->id.product = 0x0001;
	idev->id.version = 0x0001;

	idev->keycode = keymap;
	idev->keycodesize = sizeof(r8c_key_list[0].lkcode);
	idev->keycodemax = ARRAY_SIZE(r8c_key_list);

	k->idev = idev;
	k->id = id;
	k->last_code = 0x0;
	k->nb.notifier_call = r8c_key_event;
	k->nb.priority = 128;

	ret = devm_landisk_r8c_register_event_notifier(dev, &k->nb);
	if (ret)
		return ret;

	return input_register_device(idev);
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
