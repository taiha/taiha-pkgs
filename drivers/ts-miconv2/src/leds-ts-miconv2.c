// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"leds-ts-miconv2"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/leds.h>
#include <linux/reboot.h>
#include <linux/bitfield.h>

#include "ts-miconv2.h"

/*
 * commands
 */
#define MICON_CMD_LED_BRIGHT	0x3a
#  define MICON_BRIGHT_LCD_MASK	GENMASK(7,4)
#  define MICON_BRIGHT_LED_MASK	GENMASK(3,0)
#define MICON_CMD_LED_CPU	0x50
#define MICON_CMD_LED_ONOFF	0x51
#define MICON_CMD_LED_BLINK	0x52
#define MICON_CMD_LED_UNKNOWN	0x55
#define MICON_CMD_SLED_ONOFF	0x58
#define MICON_CMD_SLED_BLINK	0x59
#define MICON_CMD_LED_DLEN	2

struct micon_leds;

enum micon_led_group_type {
	MICON_LEDG_GEN  = 0, /* generic */
	MICON_LEDG_SATA,     /* sata */
};

enum micon_bright_type {
	MICON_BRIGHT_LCD = 0,
	MICON_BRIGHT_LED,
};

struct micon_led_data {
	struct led_classdev cdev;
	struct micon_leds *l;
	enum micon_led_group_type gtype;
	u8 bit;
};

struct micon_leds {
	struct miconv2 *micon;
	u8 reg_leds;
	struct micon_led_data *leds[0];
};

struct micon_led_config {
	u8 bit;
	char *name;
	enum micon_led_group_type gtype;
	bool def_on;
};

struct ts_model_led_config {
	enum ts_model_id id;
	int led_num;
	struct micon_led_config leds[32];
};

static struct micon_led_data *cdev_to_data(struct led_classdev *cdev)
{
	return container_of(cdev, struct micon_led_data, cdev);
}

static int ts_miconv2_led_set_mask(struct miconv2 *micon,
				   u8 cmd, u16 value, u16 mask)
{
	u16 led = 0;
	int ret;

	ret = ts_miconv2_read_u16(micon, cmd, &led);
	if (ret)
		return ret;
	led &= ~mask;
	led |= value;
	return ts_miconv2_write_u16(micon, cmd, led);
}

static int ts_miconv2_led_mode_set(struct led_classdev *cdev, bool on, bool blink)
{
	struct micon_led_data *data = cdev_to_data(cdev);
	u8 cmd = (data->gtype == MICON_LEDG_GEN) ?
			MICON_CMD_LED_ONOFF : MICON_CMD_SLED_ONOFF;
	u8 bcmd = (data->gtype == MICON_LEDG_GEN) ?
			MICON_CMD_LED_BLINK : MICON_CMD_SLED_BLINK;
	int ret;

	ret = ts_miconv2_led_set_mask(data->l->micon, bcmd,
				      blink ? (1 << data->bit) : 0,
				      (1 << data->bit));
	if (ret)
		return ret;
	return ts_miconv2_led_set_mask(data->l->micon, cmd,
				       on ? (1 << data-> bit) : 0,
				       (1 << data->bit));

}

static int ts_miconv2_led_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brns)
{
	return ts_miconv2_led_mode_set(cdev, (brns > 0), false);
}

static int ts_miconv2_led_blink_set(struct led_classdev *cdev,
				    unsigned long *delay_on,
				    unsigned long *delay_off)
{
	bool blink = *delay_on || *delay_off;
	return ts_miconv2_led_mode_set(cdev, blink, blink);
}

static int ts_miconv2_leds_register(struct micon_leds *l, struct device *dev,
				    struct ts_model_led_config *model)
{
	struct micon_led_data *data;
	const struct micon_led_config *config;
	u16 gen_leds = 0, gen_on_leds = 0;
	int ret, i;

	for (i = 0; i < model->led_num; i++) {
		config = &model->leds[i];
		if (!config->name)
			continue;

		data = devm_kzalloc(dev, sizeof(struct led_classdev),
				    GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		data->l = l;
		data->gtype = config->gtype;
		data->bit = config->bit;
		data->cdev.name = devm_kasprintf(dev, GFP_KERNEL,
						 "terastation:%s", config->name);
		data->cdev.brightness = config->def_on ? LED_ON : LED_OFF;
		data->cdev.brightness_set_blocking = ts_miconv2_led_brightness_set;
		data->cdev.blink_set = ts_miconv2_led_blink_set;
		l->leds[l->reg_leds] = data;
		l->reg_leds++;
		if (config->gtype == MICON_LEDG_GEN) {
			gen_leds |= 1 << config->bit;
			gen_on_leds |= config->def_on ? (1 << config->bit) : 0;
		}

		ret = devm_led_classdev_register(dev, &data->cdev);
		if (ret) {
			dev_err(dev, "failed to register LED %s\n", data->cdev.name);
			return ret;
		}

		dev_dbg(dev, "registered: %s, bit %d\n",
			 config->gtype == MICON_LEDG_GEN ? "gen" : "sata",
			 config->bit);
	}

	/* def_on LEDs: turn on, other: turn off */
	ret = ts_miconv2_led_set_mask(l->micon, MICON_CMD_LED_ONOFF,
				      gen_on_leds, gen_leds);
	if (!ret)
		/* control registered generic LEDs by MICON */
		ret = ts_miconv2_write_u16(l->micon, MICON_CMD_LED_CPU,
					   gen_leds);
	if (ret)
		dev_err(dev, "failed to set initial state of LEDs\n");

	return ret;
}

/* Device Attributes */
static ssize_t ts_miconv2_brightness_show_common(struct device *dev, char *buf,
						 enum micon_bright_type type)
{
	struct micon_leds *l = dev_get_drvdata(dev);
	ssize_t ret;
	u8 val;

	ret = ts_miconv2_read_u8(l->micon, MICON_CMD_LED_BRIGHT, &val, true);
	if (ret)
		return ret;
	val = (type == MICON_BRIGHT_LCD) ?
			FIELD_GET(MICON_BRIGHT_LCD_MASK, val) :
			FIELD_GET(MICON_BRIGHT_LED_MASK, val);
	return scnprintf(buf, 8, "%u\n", val);
}

static ssize_t lcd_brightness_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return ts_miconv2_brightness_show_common(dev, buf, MICON_BRIGHT_LCD);
}

static ssize_t led_brightness_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return ts_miconv2_brightness_show_common(dev, buf, MICON_BRIGHT_LED);
}

static ssize_t ts_miconv2_led_show_common(struct device *dev, u8 cmd, char *buf)
{
	struct micon_leds *l = dev_get_drvdata(dev);
	ssize_t ret;
	u16 val;

	ret = ts_miconv2_read_u16(l->micon, cmd, &val);
	if (ret)
		return ret;
	return scnprintf(buf, 8, "0x%04x\n", val);
}

#define MICON_LED_ATTR_SHOW(_n,_c)	\
	static ssize_t _n##_show(struct device *dev, struct device_attribute *attr, char *buf) \
	{ return ts_miconv2_led_show_common(dev, _c, buf); }

MICON_LED_ATTR_SHOW(led_cpu, MICON_CMD_LED_CPU)
MICON_LED_ATTR_SHOW(led_onoff, MICON_CMD_LED_ONOFF)
MICON_LED_ATTR_SHOW(led_blink, MICON_CMD_LED_BLINK)
MICON_LED_ATTR_SHOW(led_55, MICON_CMD_LED_UNKNOWN)
MICON_LED_ATTR_SHOW(sata_led_onoff, MICON_CMD_SLED_ONOFF)
MICON_LED_ATTR_SHOW(sata_led_blink, MICON_CMD_SLED_BLINK)

static ssize_t ts_miconv2_brightness_store_common(struct device *dev,
						  const char *buf, size_t len,
						  enum micon_bright_type type)
{
	struct micon_leds *l = dev_get_drvdata(dev);
	ssize_t ret;
	long cnvval;
	u8 val;

	ret = kstrtol(buf, 0, &cnvval);
	if (ret)
		return ret;
	if (cnvval < 0 || cnvval > 0xf)
		return -EINVAL;
	ret = ts_miconv2_read_u8(l->micon, MICON_CMD_LED_BRIGHT, &val, true);
	if (ret)
		return ret;
	val &= (type == MICON_BRIGHT_LCD) ?
			~MICON_BRIGHT_LCD_MASK :
			~MICON_BRIGHT_LED_MASK;
	val |= (type == MICON_BRIGHT_LCD) ?
			FIELD_PREP(MICON_BRIGHT_LCD_MASK, (u8)cnvval) :
			FIELD_PREP(MICON_BRIGHT_LED_MASK, (u8)cnvval);
	ret = ts_miconv2_write_u8(l->micon, MICON_CMD_LED_BRIGHT, val);
	return ret ? ret : len;
}

static ssize_t lcd_brightness_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	return ts_miconv2_brightness_store_common(dev, buf, len, MICON_BRIGHT_LCD);
}

static ssize_t led_brightness_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	return ts_miconv2_brightness_store_common(dev, buf, len, MICON_BRIGHT_LED);
}

static ssize_t ts_miconv2_led_store_common(struct device *dev, u8 cmd,
					 const char *buf, size_t len)
{
	struct micon_leds *l = dev_get_drvdata(dev);
	ssize_t ret;
	long val;

	ret = kstrtol(buf, 0, &val);
	if (ret)
		return ret;
	if (val < 0 || val > USHRT_MAX)
		return -EINVAL;
	ret = ts_miconv2_write_u16(l->micon, cmd, (u16)val);
	return ret ? ret : len;
}

#define MICON_LED_ATTR_STORE(_n,_c)	\
	static ssize_t _n##_store(struct device *dev, struct device_attribute *attr, \
				  const char *buf, size_t len) \
	{ return ts_miconv2_led_store_common(dev, _c, buf, len); }

MICON_LED_ATTR_STORE(led_cpu, MICON_CMD_LED_CPU)
MICON_LED_ATTR_STORE(led_onoff, MICON_CMD_LED_ONOFF)
MICON_LED_ATTR_STORE(led_blink, MICON_CMD_LED_BLINK)
MICON_LED_ATTR_STORE(led_55, MICON_CMD_LED_UNKNOWN)
MICON_LED_ATTR_STORE(sata_led_onoff, MICON_CMD_SLED_ONOFF)
MICON_LED_ATTR_STORE(sata_led_blink, MICON_CMD_SLED_BLINK)

static DEVICE_ATTR_RW(lcd_brightness);
static DEVICE_ATTR_RW(led_brightness);
static DEVICE_ATTR_RW(led_cpu);
static DEVICE_ATTR_RW(led_onoff);
static DEVICE_ATTR_RW(led_blink);
static DEVICE_ATTR_RW(led_55);
static DEVICE_ATTR_RW(sata_led_onoff);
static DEVICE_ATTR_RW(sata_led_blink);

static struct attribute *ts_micon_leds_attrs[] = {
	&dev_attr_lcd_brightness.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_cpu.attr,
	&dev_attr_led_onoff.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_55.attr,
	&dev_attr_sata_led_onoff.attr,
	&dev_attr_sata_led_blink.attr,
	NULL,
};

static const struct attribute_group ts_micon_leds_attr_group = {
	.attrs = ts_micon_leds_attrs,
};

#define MICON_GEN_LED(_b,_n,_on)	\
	{.bit=_b,.name=_n,.gtype=MICON_LEDG_GEN,.def_on=_on}
#define MICON_SATA_LED(_b,_n,_on)	\
	{.bit=_b,.name=_n,.gtype=MICON_LEDG_SATA,.def_on=on}

static struct ts_model_led_config ts_model_list[] = {
	[ID_TS3400D] = {
		.id = ID_TS3400D,
		.led_num = 14,
		.leds = {
			MICON_GEN_LED(0,  "lcd-red"  , false),
			MICON_GEN_LED(1,  "lcd-green", true),
			MICON_GEN_LED(2,  "lcd-blue" , false),
			MICON_GEN_LED(4,  "hdd1-red" , false),
			MICON_GEN_LED(5,  "hdd2-red" , false),
			MICON_GEN_LED(6,  "hdd3-red" , false),
			MICON_GEN_LED(7,  "hdd4-red" , false),
			MICON_GEN_LED(8,  "power"    , true),
			MICON_GEN_LED(9,  "info"     , false),
			MICON_GEN_LED(10, "error"    , false),
			MICON_GEN_LED(11, "power2"   , false),
			MICON_GEN_LED(12, "function" , false),
			MICON_GEN_LED(13, "lan1"     , false),
			MICON_GEN_LED(14, "lan2"     , false),
			/* SATA LED commands (0x58,0x59) are unavailable */
		},
	},
};

static int ts_miconv2_leds_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct micon_child_pdata *pdata = dev_get_platdata(dev);
	struct ts_model_led_config *model;
	struct miconv2 *micon = dev_get_drvdata(dev->parent);
	struct micon_leds *l;
	int ret;

	pr_info("MICON v2 LED driver\n");

	if (pdata->id >= ARRAY_SIZE(ts_model_list)) {
		dev_err(dev, "this TeraStation is not supported by LED driver\n");
		return -ENODEV;
	}
	model = &ts_model_list[pdata->id];

	l = devm_kzalloc(dev,
			 struct_size(l, leds, model->led_num),
			 GFP_KERNEL);
	if (!l) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, l);
	l->micon = micon;

	ret = ts_miconv2_leds_register(l, dev, model);
	if (ret)
		return ret;
	dev_info(dev, "%d LEDs registered\n", l->reg_leds);

	ret = sysfs_create_group(&dev->kobj, &ts_micon_leds_attr_group);
	if (ret)
		return ret;

	return 0;
}

static int ts_miconv2_leds_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &ts_micon_leds_attr_group);
	return 0;
}

static const struct of_device_id ts_miconv2_leds_ids[] = {
	{ .compatible = "buffalo,ts-miconv2-leds" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, hdl_micon_leds_ids);

static struct platform_driver ts_miconv2_leds_driver = {
	.probe = ts_miconv2_leds_probe,
	.remove = ts_miconv2_leds_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ts_miconv2_leds_ids,
	},
};
module_platform_driver(ts_miconv2_leds_driver);

MODULE_DESCRIPTION("MICON v2 LEDs driver for Buffalo TeraStation series");
MODULE_LICENSE("GPL v2");
