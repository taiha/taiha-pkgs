// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"ts-miconv2-hwmon"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/hwmon.h>
#include <linux/thermal.h>

#include "ts-miconv2.h"

#define MICON_CMD_FAN_CTRL	0x33
#define MICON_CMD_FAN_SPD	0x38
#define MICON_CMD_TEMP		0x37

/* actual range: -55 - 125, but use 0 - 125 for MICON error */
#define MICON_TEMP_MAX		125
#define MICON_TEMP_MIN		0

struct micon_hwmon {
	struct miconv2 *micon;
	int fan_mode;
};

enum micon_fan_mode {
	MICON_FAN_STOP = 0,
	MICON_FAN_LOW,
	MICON_FAN_MID,
	MICON_FAN_HIGH,
	MICON_FAN_MAX,

	MICON_FAN_INVL = 15,
};

static int ts_miconv2_hwmon_get(struct micon_hwmon *h, long *val, u8 cmd, bool err)
{
	int ret;
	u8 _val;

	ret = ts_miconv2_read_u8(h->micon, cmd, &_val, err);
	if (ret)
		return ret;
	pr_debug("%s: _val=%u (0x%02x)\n", __func__, _val, _val);
	*val = _val;
	return 0;
}

static int ts_miconv2_hwmon_read(struct device *dev,
			    enum hwmon_sensor_types type,
			    u32 attr, int chan, long *val)
{
	struct micon_hwmon *h = dev_get_drvdata(dev);
	int ret;

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			ret = ts_miconv2_hwmon_get(h, val, MICON_CMD_TEMP, false);
			if (ret)
				return ret;
			/* convert to millicelsius */
			*val *= 1000;
			return (val < MICON_TEMP_MIN) ? -EINVAL : 0;
		}
		break;
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_input:
			ret = ts_miconv2_hwmon_get(h, val, MICON_CMD_FAN_SPD, true);
			if (ret)
				return ret;
			/*
			 * MICON_CMD_FAN_SPD -> retval = <revo./3sec> * 2
			 * convert to rpm
			 */
			*val *= 10;
			return 0;
		case hwmon_fan_target:
			return ts_miconv2_hwmon_get(h, val, MICON_CMD_FAN_CTRL, false);
		}
	default:
		break;
	}

	return -EINVAL;
}

static int ts_miconv2_fanmode_set(struct micon_hwmon *h, long val)
{
	int ret;

	if (val < MICON_FAN_STOP || val > MICON_FAN_HIGH)
		return -EINVAL;

	ret = ts_miconv2_write_u8(h->micon, MICON_CMD_FAN_CTRL, (u8)val);
	if (ret)
		return ret;
	h->fan_mode = (int)val;
	return 0;
}

static int ts_miconv2_hwmon_write(struct device *dev,
			   enum hwmon_sensor_types type,
			   u32 attr, int chan, long val)
{
	struct micon_hwmon *h = dev_get_drvdata(dev);

	if (type != hwmon_fan || attr != hwmon_fan_target)
		return -EINVAL;

	return ts_miconv2_fanmode_set(h, val);
}

static umode_t ts_miconv2_hwmon_is_visible(const void *data,
					   enum hwmon_sensor_types type,
					   u32 attr, int chan)
{
	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			return 0444;
		default:
			break;
		}
		break;
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_input:
			return 0444;
			break;
		case hwmon_fan_target:
			return 0644;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return 0;
}

static const struct hwmon_channel_info *ts_miconv2_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT),
	HWMON_CHANNEL_INFO(fan,  HWMON_F_INPUT | HWMON_F_TARGET),
	NULL
};

static const struct hwmon_ops ts_miconv2_hwmon_ops = {
	.is_visible = ts_miconv2_hwmon_is_visible,
	.read = ts_miconv2_hwmon_read,
	.write = ts_miconv2_hwmon_write,
};

static const struct hwmon_chip_info micon_hwmon_chip_info = {
	.ops = &ts_miconv2_hwmon_ops,
	.info = ts_miconv2_hwmon_info,
};

static int ts_miconv2_fan_get_max_state(struct thermal_cooling_device *cldev,
					unsigned long *state)
{
	*state = MICON_FAN_HIGH;
	return 0;
}

static int ts_miconv2_fan_get_cur_state(struct thermal_cooling_device *cldev,
					unsigned long *state)
{
	struct micon_hwmon *h = cldev->devdata;

	if (!h)
		return -EINVAL;

	*state = h->fan_mode;
	return 0;
}

static int ts_miconv2_fan_set_cur_state(struct thermal_cooling_device *cldev,
					unsigned long state)
{
	struct micon_hwmon *h = cldev->devdata;

	if (!h)
		return -EINVAL;

	return ts_miconv2_fanmode_set(h, (long)state);
}

static const struct thermal_cooling_device_ops ts_miconv2_fan_cool_ops = {
	.get_max_state = ts_miconv2_fan_get_max_state,
	.get_cur_state = ts_miconv2_fan_get_cur_state,
	.set_cur_state = ts_miconv2_fan_set_cur_state,
};

static int ts_miconv2_hwmon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct miconv2 *micon = dev_get_drvdata(dev->parent);
	struct device *hmdev;
	struct thermal_cooling_device *cldev;
	struct micon_hwmon *h;
	int ret;
	u8 val;

	pr_info("MICON v2 hwmon driver\n");

	h = devm_kzalloc(dev, sizeof(*h), GFP_KERNEL);
	if (!h) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, h);

	h->micon = micon;
	ret = ts_miconv2_read_u8(micon, MICON_CMD_FAN_CTRL, &val, false);
	if (ret)
		return ret;
	h->fan_mode = val;

	hmdev = devm_hwmon_device_register_with_info(dev, "ts_miconv2_hwmon", h,
						     &micon_hwmon_chip_info, NULL);
	if (IS_ERR(hmdev))
		return PTR_ERR(hmdev);
	cldev = devm_thermal_of_cooling_device_register(dev, NULL, DRV_NAME, h, 
							&ts_miconv2_fan_cool_ops);
	if (IS_ERR(cldev))
		dev_err(dev, "failed to register cooling device\n");

	return 0;
}

static const struct of_device_id ts_miconv2_hwmon_ids[] = {
	{ .compatible = "buffalo,ts-miconv2-hwmon" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, ts_miconv2_hwmon_ids);

static struct platform_driver ts_miconv2_hwmon_driver = {
	.probe = ts_miconv2_hwmon_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ts_miconv2_hwmon_ids,
	},
};
module_platform_driver(ts_miconv2_hwmon_driver);

MODULE_DESCRIPTION("MICON v2 hwmon driver for Buffalo TeraStation series");
MODULE_LICENSE("GPL v2");
