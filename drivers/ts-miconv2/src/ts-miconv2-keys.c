// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"ts-miconv2-keys"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/bitfield.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#include "ts-miconv2.h"

#define MICON_CMD_INT_SW_STAT	0x36
#define MICON_SW_STAT_KEY_MASK	0x1f

#define MICON_KEY_POLL_INTERVAL	450

#define MICON_PRSD_KEY(_rdval)	FIELD_GET(MICON_SW_STAT_KEY_MASK, ~_rdval)

enum micon_key_bit {
	MICON_KEY_POWER = 0,
	MICON_KEY_FUNC,
	MICON_KEY_DISP,
	MICON_KEY_RESET,
	MICON_KEY_INVAL = 7,
};

struct micon_key_data {
	enum micon_key_bit bit;
	unsigned short lkcode;
	char *name;
};

struct micon_keys {
	struct miconv2 *micon;
	struct delayed_work work;
	struct delayed_work poll_work;
	struct input_dev *idev;
	int pressed;
	unsigned long poll_delay;
};

static struct micon_key_data micon_key_list[] = {
	[MICON_KEY_POWER] = {
		.bit    = MICON_KEY_POWER,
		.lkcode = KEY_POWER,
		.name   = "power",
	},
	[MICON_KEY_FUNC] = {
		.bit    = MICON_KEY_FUNC,
		.lkcode = BTN_0,
		.name   = "func",
	},
	[MICON_KEY_DISP] = {
		.bit    = MICON_KEY_DISP,
		.lkcode = BTN_1,
		.name   = "display",
	},
	[MICON_KEY_RESET] = {
		.bit    = MICON_KEY_RESET,
		.lkcode = KEY_RESTART,
		.name   = "reset",
	},
};

static void ts_miconv2_keys_report_key(struct micon_keys *k, unsigned short lkcode,
				       bool pressed, const char *name)
{
	pr_debug("%s %s, lkcode=%u (0x%x)\n",
		name, pressed ? "pressed" : "released", lkcode, lkcode);
	input_report_key(k->idev, lkcode, pressed ? 1 : 0);
	input_sync(k->idev);
}

/*
 * Poll for key release
 * polling code based on drivers/input/input-poller.c
 */
static void ts_miconv2_keys_poll_work(struct work_struct *work)
{
	struct micon_keys *k = container_of(work, struct micon_keys,
					    poll_work.work);
	struct micon_key_data *key;
	u8 pressed;
	int ret;

	ret = ts_miconv2_read_u8(k->micon, MICON_CMD_INT_SW_STAT, &pressed, false);
	if (ret)
		return;

	pressed = MICON_PRSD_KEY(pressed);
	pr_debug("%s: pressed-> 0x%02x, k->pressed-> %d\n",
		__func__, pressed, k->pressed);
	/*
	 * queue work and exit when the sw_stat value
	 * hasn't been changed (pressed)
	 */
	if (pressed == (1 << k->pressed)) {
		/* continue queueing the work when some key is pressed */
		queue_delayed_work(system_freezable_wq,
				   &k->poll_work, k->poll_delay);
		return;
	}

	key = &micon_key_list[k->pressed];
	ts_miconv2_keys_report_key(k, key->lkcode, false, key->name);
	k->pressed = MICON_KEY_INVAL;
}

static void ts_miconv2_keys_work(struct work_struct *work)
{
	struct micon_keys *k = container_of(work, struct micon_keys,
					    work.work);
	struct micon_key_data *key;
	u8 pressed;
	int ret, i;

	/*
	 * multiple keys cannot be handled by MICON
	 * at the same time, report "release"
	 */
	if (k->pressed != MICON_KEY_INVAL) {
		key = &micon_key_list[k->pressed];
		ts_miconv2_keys_report_key(k, key->lkcode, false, key->name);
		/* cancel running work */
		cancel_delayed_work_sync(&k->poll_work);
		k->pressed = MICON_KEY_INVAL;
	}

	ret = ts_miconv2_read_u8(k->micon, MICON_CMD_INT_SW_STAT, &pressed, false);
	if (ret)
		return;

	pr_debug("%s: sw_stat=0x%02x\n", __func__, pressed);
	pressed = MICON_PRSD_KEY(pressed);
	for (i = 0; i < ARRAY_SIZE(micon_key_list); i++) {
		key = &micon_key_list[i];
		/* skip un-pressed keys */
		if (!(pressed & (1 << key->bit)))
			continue;

		ts_miconv2_keys_report_key(k, key->lkcode, true, key->name);
		k->pressed = key->bit;

		/* start queueing the work when some key is pressed */
		queue_delayed_work(system_freezable_wq,
				   &k->poll_work, k->poll_delay);
		break;
	}
}

static irqreturn_t ts_miconv2_keys_irq_handler(int irq, void *data)
{
	struct micon_keys *k = data;

	mod_delayed_work(system_wq, &k->work, 0);
	return IRQ_HANDLED;
}

static int ts_miconv2_keys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct micon_child_pdata *pdata = dev_get_platdata(dev);
	struct micon_keys *k;
	struct input_dev *idev;
	unsigned short *keymap;
	int i, ret;

	pr_info("MICON v2 Key driver\n");

	k = devm_kzalloc(dev, sizeof(*k), GFP_KERNEL);
	if (!k)
		return -ENOMEM;
	k->micon = dev_get_drvdata(pdev->dev.parent);
	k->pressed = MICON_KEY_INVAL;
	k->poll_delay = msecs_to_jiffies(MICON_KEY_POLL_INTERVAL);
	if (k->poll_delay >= HZ)
		k->poll_delay = round_jiffies_relative(k->poll_delay);
	pr_debug("set polling delay to %lu\n", k->poll_delay);

	/* reuqest irq */
	ret = devm_request_irq(dev, pdata->irq, ts_miconv2_keys_irq_handler,
			       0, DRV_NAME, k);
	if (ret)
		return ret;

	/* allocate keycodes */
	keymap = devm_kcalloc(&pdev->dev, ARRAY_SIZE(micon_key_list),
			      sizeof(micon_key_list[0].lkcode), GFP_KERNEL);
	if (!keymap)
		return -ENOMEM;

	/* allocate input device */
	idev = devm_input_allocate_device(dev);
	if (!idev)
		return -ENOMEM;

	platform_set_drvdata(pdev, k);
	input_set_drvdata(idev, k);
	INIT_DELAYED_WORK(&k->work, ts_miconv2_keys_work);
	INIT_DELAYED_WORK(&k->poll_work, ts_miconv2_keys_poll_work);

	for (i = 0; i < ARRAY_SIZE(micon_key_list); i++) {
		keymap[i] = micon_key_list[i].lkcode;
		input_set_capability(idev, EV_KEY, keymap[i]);
	}

	idev->name = pdev->name;
	idev->phys = "ts-miconv2-keys/input0";
	idev->dev.parent = &pdev->dev;
	idev->id.bustype = BUS_HOST;
	idev->id.vendor = 0x0001;
	idev->id.product = 0x0001;
	idev->id.version = 0x0100;

	idev->keycode = keymap;
	idev->keycodesize = sizeof(micon_key_list[0].lkcode);
	idev->keycodemax = ARRAY_SIZE(micon_key_list);

	k->idev = idev;

	return input_register_device(idev);
}

static const struct of_device_id ts_miconv2_keys_ids[] = {
	{ .compatible = "buffalo,ts-miconv2-keys" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ts_miconv2_keys_ids);

static struct platform_driver ts_miconv2_keys_driver = {
	.probe = ts_miconv2_keys_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ts_miconv2_keys_ids,
	},
};
module_platform_driver(ts_miconv2_keys_driver);

MODULE_DESCRIPTION("MICON v2 Key driver for Buffalo TeraStation series");
MODULE_LICENSE("GPL v2");
