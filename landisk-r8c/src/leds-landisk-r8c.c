// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"leds-landisk-r8c"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/leds.h>
#include <linux/reboot.h>
#include <linux/bitfield.h>

#include "landisk-r8c.h"

/* default is 100 */
#define R8C_LED_BRNS_MAX	100
#define R8C_LED_BRNS_MIN	0

/*
 * commands
 *
 * - LED: brightness setting for all (not for each)
 * - STS: mode setting for status LED
 *   - this LED doesn't have off-state mode and has Red/Green color but
 *     cannot be controlled separately, so registration to Linux is not
 *     good, export only custom sysfs
 * - HDD: mode setting for HDD LED
 */
#define CMD_LED			"led"
#define CMD_STS			"sts"
#define CMD_HDD			"hdd"

#define LED_CMD_LEN		3

struct r8c_leds;

struct r8c_led_data {
	struct led_classdev cdev;
	struct r8c_leds *l;
	u8 port;
};

struct r8c_status_mode {
	int id;
	char *name;
};

struct r8c_leds {
	struct r8c_mcu *r8c;
	struct notifier_block nb;
	u8 reg_leds;
	struct r8c_led_data *leds[0];
};

static struct r8c_led_data *cdev_to_data(struct led_classdev *cdev)
{
	return container_of(cdev, struct r8c_led_data, cdev);
}

static int r8c_led_mode_set(struct led_classdev *cdev,
			     enum led_brightness brns, bool blink)
{
	struct r8c_led_data *data = cdev_to_data(cdev);
	const char *arg = devm_kasprintf(cdev->dev, GFP_KERNEL, "%d %d",
					 data->port,
					 blink ? 1 : (brns ? 5 : 0));

	return landisk_r8c_exec_cmd(data->l->r8c, CMD_HDD, arg, NULL, 0);
}

static void r8c_led_brightness_set(struct led_classdev *cdev,
				    enum led_brightness brns)
{
	r8c_led_mode_set(cdev, brns, false);
}

static int r8c_led_blink_set(struct led_classdev *cdev,
			      unsigned long *delay_on,
			      unsigned long *delay_off)
{
	if (*delay_on == 0 && *delay_off == 0)
		return 0;

	return r8c_led_mode_set(cdev, LED_ON, true);
}

static int r8c_leds_register(struct r8c_leds *l, struct device *dev,
			     u8 max_leds)
{
	struct r8c_led_data *data;
	int ret, i;

	for (i = 0; i < max_leds; i++) {

		data = devm_kzalloc(dev, sizeof(struct led_classdev),
				    GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		data->l = l;
		data->port = i;
		data->cdev.name = devm_kasprintf(dev, GFP_KERNEL,
						 "landisk:hdd%d", i + 1);
		data->cdev.brightness = LED_OFF;
		data->cdev.brightness_set = r8c_led_brightness_set;
		data->cdev.blink_set = r8c_led_blink_set;
		l->leds[l->reg_leds] = data;
		l->reg_leds++;

		ret = devm_led_classdev_register(dev, &data->cdev);
		if (ret) {
			dev_err(dev, "failed to register LED, port %d\n", i);
			return -ENODEV;
		}

		dev_dbg(dev, "registered: port %d\n", i);
	}

	return 0;
}

/* HDL-XR/XV seems to have "notice" mode (id=4) */
static const struct r8c_status_mode status_mode_list[] = {
	{
		.id = 0, .name = "on",
	},
	{
		.id = 1, .name = "blink",
	},
	{
		.id = 2, .name = "err",
	},
	{
		.id = 5, .name = "notify",
	},
};

static ssize_t status_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct r8c_leds *l = dev_get_drvdata(dev);
	ssize_t ret, totlen = 0;
	char cmdbuf[4];
	long mode_id;
	int i;

	ret = landisk_r8c_exec_cmd(l->r8c, CMD_STS, NULL, cmdbuf, 4);
	if (ret <= 0)
		return 0;

	ret = kstrtol(cmdbuf, 16, &mode_id);
	if (ret) {
		dev_err(dev, "failed to parse \"%s\" (%d)\n", cmdbuf, ret);
		return 0;
	};

	for (i = 0; i < 4; i++) {
		const struct r8c_status_mode *mode = &status_mode_list[i];

		ret = scnprintf(buf + totlen, 10, "%s%s%s ",
				!!(mode_id == mode->id) ? "[" : "",
				mode->name,
				!!(mode_id == mode->id) ? "]" : "");

		totlen += ret;
	}

	buf[totlen] = '\n';
	totlen++;

	return totlen;
}

static ssize_t status_mode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct r8c_leds *l = dev_get_drvdata(dev);
	ssize_t ret;
	const char *mode_name = NULL;
	int i;

	/* check minimum length */
	if (len < 2)
		return -EINVAL;

	for (i = 0; i < 5; i++) {
		const struct r8c_status_mode *mode = &status_mode_list[i];

		if (!strncmp(buf, mode->name, strlen(mode->name))) {
			mode_name = mode->name;
			break;
		}
	}

	if (!mode_name) {
		dev_dbg(dev, "specified mode didn't match\n");
		return -EINVAL;
	}

	ret = landisk_r8c_exec_cmd(l->r8c, CMD_STS, mode_name, NULL, 0);

	return ret ? ret : len;
}

static DEVICE_ATTR_RW(status_mode);

static ssize_t leds_brightness_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct r8c_leds *l = dev_get_drvdata(dev);
	ssize_t ret;
	char cmdbuf[4];
	/* brightness (%) */
	long brightness;

	ret = landisk_r8c_exec_cmd(l->r8c, CMD_LED, NULL, cmdbuf, 4);
	if (ret <= 0 || !strlen(cmdbuf))
		return 0;

	/* R8C returns hex value */
	ret = kstrtol(cmdbuf, 16, &brightness);
	if (ret) {
		dev_err(dev,
			"failed to parse \"%s\" (%d)\n", cmdbuf, ret);
		return 0;
	}

	return scnprintf(buf, 10, "%ld %%\n", brightness);
}

static ssize_t leds_brightness_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct r8c_leds *l = dev_get_drvdata(dev);
	ssize_t ret;
	/* brightness (%) */
	long brightness;

	ret = kstrtol(buf, 0, &brightness);
	if (ret)
		return ret;

	if (brightness < R8C_LED_BRNS_MIN ||
	    R8C_LED_BRNS_MAX < brightness)
		return -EINVAL;

	ret = landisk_r8c_exec_cmd(l->r8c, CMD_LED, buf, NULL, 0);

	return ret ? ret : len;
}
/* for all LEDs, not for each */
static DEVICE_ATTR_RW(leds_brightness);

static struct attribute *r8c_leds_attrs[] = {
	&dev_attr_status_mode.attr,
	&dev_attr_leds_brightness.attr,
	NULL,
};

static const struct attribute_group r8c_leds_attr_group = {
	.attrs = r8c_leds_attrs,
};

static int r8c_leds_shutdown(struct notifier_block *nb,
			     unsigned long mode, void *cmd)
{
	struct r8c_leds *l = container_of(nb, struct r8c_leds, nb);
	int i;

	pr_info("Shutdown LEDs\n");
	for (i = 0; i < l->reg_leds; i++)
		if (l->leds[i])
			led_set_brightness(&l->leds[i]->cdev, LED_OFF);

	return NOTIFY_DONE;
}

static const u8 model_hdd_leds[] = {
	[ID_HDL_A]  = 0,
	[ID_HDL2_A] = 2,
};

static int landisk_r8c_leds_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct r8c_mcu *r8c = dev_get_drvdata(dev->parent);
	struct r8c_leds *l;
	u32 model_id = *(u32 *)dev_get_platdata(dev);
	int ret;
	u8 hdd_leds;

	pr_info("R8C LED driver\n");

	if (model_id > ID_HDL_MAX) {
		dev_err(dev, "unsupported model id %d\n", model_id);
		return -ENOTSUPP;
	}
	dev_dbg(dev, "got model id %d\n", model_id);

	hdd_leds = model_hdd_leds[model_id];

	/* only HDD LEDs will be registered */
	l = devm_kzalloc(dev, struct_size(l, leds, hdd_leds),
			 GFP_KERNEL);
	if (!l) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, l);

	l->r8c = r8c;

	ret = sysfs_create_group(&dev->kobj, &r8c_leds_attr_group);
	if (ret)
		return ret;

	/* HDL-A has no HDD LED */
	if (!hdd_leds)
		return 0;

	ret = r8c_leds_register(l, dev, hdd_leds);
	if (ret)
		goto err;

	l->nb.notifier_call = r8c_leds_shutdown;
	l->nb.priority = 128;

	ret = devm_register_reboot_notifier(dev, &l->nb);
	if (ret) {
		dev_err(dev, "failed to register LED shutdown\n");
		goto err;
	}

	/* set "on" to status LED */
	landisk_r8c_exec_cmd(l->r8c, CMD_STS, "on", NULL, 0);

	return 0;

err:
	sysfs_remove_group(&dev->kobj, &r8c_leds_attr_group);

	return ret;
}

static int landisk_r8c_leds_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &r8c_leds_attr_group);

	return 0;
}

static const struct of_device_id landisk_r8c_leds_ids[] = {
	{ .compatible = "iodata,landisk-r8c-leds" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, hdl_r8c_leds_ids);

static struct platform_driver landisk_r8c_leds_driver = {
	.probe = landisk_r8c_leds_probe,
	.remove = landisk_r8c_leds_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = landisk_r8c_leds_ids,
	},
};
module_platform_driver(landisk_r8c_leds_driver);

MODULE_DESCRIPTION("R8C LED driver for I-O DATA LAN DISK series");
MODULE_LICENSE("GPL v2");
