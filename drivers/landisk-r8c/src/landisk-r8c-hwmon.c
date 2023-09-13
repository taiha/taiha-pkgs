// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"landisk-r8c-hwmon"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#include "landisk-r8c.h"

#define R8C_TEMP_MAX		127
#define R8C_TEMP_MIN		0

/*
 * return val by fancont command
 *
 *   ex.: 12
 *        1--> Manual (0-> Auto)
 *         2-> Speed 2
 */
#define R8C_FANCTL_MAN_MASK	0xf0U
#define R8C_FANCTL_SPD_MASK	0xfU

/* fancont setting */
#define R8C_FANCTL_MOD_MASK	0xf0U

#define FANCTL_MOD(x)		(x << 4)

/* threshold indexes */
#define R8C_THLD_FANSPD_2ND	0
#define R8C_THLD_FANSPD_1ST	1
#define R8C_THLD_TEMP_CRIT	2

#define R8C_THLD_IDX_MAX	R8C_THLD_TEMP_CRIT

#define TH_IDX_OFFS(i)		((R8C_THLD_IDX_MAX - i) * 8)
#define TH_IDX_MASK(i)		(0xffU << TH_IDX_OFFS(i))

/* commands */
#define CMD_FANCTL		"fancont"  /* get/set fan speed mode */
#define CMD_TEMP		"temp"	   /* get current temperature */
#define CMD_THGET		"thget"	   /* get temperature threshold */
#define CMD_THSET		"thset"	   /* set temperature threshold */

/* events from R8C */
#define R8C_EV_FANERR		'F'
#define R8C_EV_TMPERR		'T'

struct landisk_model_info {
	u32 model_id;
	const struct attribute_group **attr_grp;
	u8 fan_spd_num;
	const u8 *fan_spds;
};

struct r8c_hwmon {
	struct r8c_mcu *r8c;
	struct notifier_block nb;
	const struct landisk_model_info *model;
};

struct r8c_fan_mode {
	u8 mode_id;
	char *name;
	u32 models;
};

enum {
	FAN_MOD_STOP = 0,
	FAN_MOD_LOW,
	FAN_MOD_MID,
	FAN_MOD_HIGH,
	FAN_MOD_AUTO,
	FAN_MOD_MAX,

	FAN_MOD_INVL = 15,
};

/* pass: millicelsius */
static int r8c_thld_set(struct r8c_hwmon *h, int index, long temp)
{
	char cmdbuf[16], retbuf[4];
	int ret;

	temp /= 1000;
	if (temp <= R8C_TEMP_MIN || R8C_TEMP_MAX < temp)
		return -EINVAL;

	scnprintf(cmdbuf, 16, "%u %ld", index, temp);

	ret = landisk_r8c_exec_cmd(h->r8c, CMD_THSET, cmdbuf, retbuf, 4);
	/* R8C returns '1' when failed to set value */
	if (ret <= 0 || retbuf[0] == '1') {
		pr_err("failed to set threshold\n");
		return ret;
	}

	return 0;
}

/* return: celsius */
static int r8c_thld_get(struct r8c_hwmon *h, u32 *val)
{
	char retbuf[12];
	u32 spd1, spd2, terr;
	int ret;

	ret = landisk_r8c_exec_cmd(h->r8c, CMD_THGET, NULL, retbuf, 12);
	if (ret <= 0) {
		pr_err("failed to get threshold\n");
		return ret;
	}

	ret = sscanf(retbuf, "%02x %02x %02x", &spd2, &spd1, &terr);
	if (ret != 3)
		return -EINVAL;

	*val = FIELD_PREP(0xff0000, spd2) | FIELD_PREP(0xff00, spd1) |
	       FIELD_PREP(0xff, terr);

	return 0;
}

/* return: millicelsius */
static int r8c_temp_get(struct r8c_hwmon *h, long *temp)
{
	char retbuf[4];
	int ret;

	ret = landisk_r8c_exec_cmd(h->r8c, CMD_TEMP, NULL, retbuf, 4);
	if (ret <= 0) {
		pr_err("failed to get temperature\n");
		return ret;
	}

	ret = kstrtol(retbuf, 16, temp);
	if (ret)
		return ret;
	*temp *= 1000;

	return 0;
}

static int r8c_hwmon_read(struct device *dev,
			  enum hwmon_sensor_types type,
			  u32 attr, int chan, long *val)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);
	int ret;

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			return r8c_temp_get(h, val);
		case hwmon_temp_crit:
			u32 thld;

			ret = r8c_thld_get(h, &thld);
			if (ret)
				return ret;

			*val = FIELD_GET(TH_IDX_MASK(R8C_THLD_TEMP_CRIT),
					 thld);
			*val *= 1000;
			return 0;
		}
		break;
	default:
		break;
	}

	return -EINVAL;
}

static int r8c_hwmon_write(struct device *dev,
			   enum hwmon_sensor_types type,
			   u32 attr, int chan, long val)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);

	if (type == hwmon_temp && attr == hwmon_temp_crit)
		return r8c_thld_set(h, R8C_THLD_TEMP_CRIT, val);

	return -EINVAL;
}

static umode_t r8c_hwmon_is_visible(const void *data,
				    enum hwmon_sensor_types type,
				    u32 attr, int chan)
{
	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			return 0444;
		case hwmon_temp_crit:
			return 0644;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return 0;
}

/* show: millicelsius */
static ssize_t fan1_temp_threshold_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);
	int index = (to_sensor_dev_attr(attr))->index;
	u32 thld;
	int ret;

	ret = r8c_thld_get(h, &thld);
	if (ret)
		return 0;

	thld = (thld & TH_IDX_MASK(index)) >> TH_IDX_OFFS(index);
	thld *= 1000;

	return scnprintf(buf, 8, "%u\n", thld);
}

/* pass: millicelsius */
static ssize_t fan1_temp_threshold_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);
	int index = (to_sensor_dev_attr(attr))->index;
	u32 cur_thld, thld;
	ssize_t ret;

	/* millicelsius */
	ret = kstrtou32(buf, 0, &thld);
	if (ret)
		return -EINVAL;

	/* limitation: 1st_threshold < 2nd_threshold */
	ret = r8c_thld_get(h, &cur_thld);
	if (ret)
		return -EINVAL;

	/* if setting 2nd threshold */
	if (index == 0 && thld <= (FIELD_GET(0xff00, cur_thld) * 1000)) {
		dev_err(dev,
			"please specify higher value than threashold1\n");
		return -EINVAL;
	}

	/* if setting 1st threshold */
	if (index == 1 && (FIELD_GET(0xff0000, cur_thld) * 1000) <= thld) {
		if (h->model->fan_spd_num <= 3) {
			ret = r8c_thld_set(h, R8C_THLD_FANSPD_2ND,
					   thld + 1000);
			if (ret)
				return -EINVAL;
		} else {
			dev_err(dev,
				"please specify lower value than threashold2\n");
			return -EINVAL;
		}
	}

	ret = r8c_thld_set(h, index, thld);

	return ret ? ret : len;
}

static SENSOR_DEVICE_ATTR_RW(fan1_temp_threshold1, fan1_temp_threshold, 1);

static const struct r8c_fan_mode fan_mode_list[] = {
	[FAN_MOD_STOP] = {
		.mode_id = FAN_MOD_STOP,
		.name    = "stop",
		.models  = BIT(ID_HDL_A) | BIT(ID_HDL2_A),
	},
	[FAN_MOD_LOW]  = {
		.mode_id = FAN_MOD_LOW,
		.name    = "low",
		.models  = BIT(ID_HDL2_A),
	},
	[FAN_MOD_MID]  = {
		.mode_id = FAN_MOD_MID,
		.name    = "middle",
		.models  = BIT(ID_HDL_A),
	},
	[FAN_MOD_HIGH] = {
		.mode_id = FAN_MOD_HIGH,
		.name    = "high",
		.models  = BIT(ID_HDL2_A),
	},
	[FAN_MOD_AUTO] = {
		.mode_id = FAN_MOD_AUTO,
		.name    = "auto",
		.models  = BIT(ID_HDL_A) | BIT(ID_HDL2_A),
	},
};

static ssize_t fan1_avail_modes_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);
	ssize_t totlen = 0;
	int i;

	for (i = 0; i < FAN_MOD_MAX; i++) {
		if (BIT(h->model->model_id) & fan_mode_list[i].models)
			totlen += scnprintf(buf + totlen, 8, "%s ",
					    fan_mode_list[i].name);
	}

	buf[totlen] = '\n';
	totlen++;

	return totlen;
}

static DEVICE_ATTR_RO(fan1_avail_modes);

static ssize_t fan1_mode_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);
	ssize_t totlen = 0;
	char retbuf[4];
	const char *mode_str;
	u32 mode;
	int ret, i;

	ret = landisk_r8c_exec_cmd(h->r8c, CMD_FANCTL, NULL, retbuf, 4);
	if (ret <= 0)
		return 0;

	ret = kstrtou32(retbuf, 16, &mode);
	if (ret) {
		dev_err(dev, "failed to parse \"%s\" (%d)\n",
			retbuf, ret);
		return 0;
	}

	totlen += scnprintf(buf, 8, "%s-",
			    FIELD_GET(R8C_FANCTL_MAN_MASK, mode) ?
				"manual" : "auto");

	for (i = 0; i < h->model->fan_spd_num + 1; i++) {
		u8 fan_spd = h->model->fan_spds[i];

		if (FIELD_GET(R8C_FANCTL_SPD_MASK, mode)
		      == FIELD_GET(R8C_FANCTL_SPD_MASK, fan_spd))
			mode_str = fan_mode_list[
					FIELD_GET(R8C_FANCTL_MOD_MASK,
						  fan_spd)].name;
	}

	if (!mode_str) {
		dev_err(dev, "unknown fan mode: %02x", mode);
		return 0;
	}

	totlen += scnprintf(buf + totlen, 8, "%s\n", mode_str);

	return totlen;
}

static ssize_t fan1_mode_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);
	ssize_t ret;
	u8 mode_id = FAN_MOD_INVL, spd = 15;
	char cmdbuf[4];
	int i;

	for (i = 0; i < FAN_MOD_MAX; i++) {
		const struct r8c_fan_mode *mode = &fan_mode_list[i];

		if (BIT(h->model->model_id) & mode->models &&
		    !strncmp(buf, mode->name, strlen(mode->name)))
			mode_id = mode->mode_id;
	}

	if (mode_id == FAN_MOD_INVL)
		return -EINVAL;

	for (i = 0; i < h->model->fan_spd_num + 1; i++) {
		u8 _spd = h->model->fan_spds[i];

		if (FIELD_GET(R8C_FANCTL_MOD_MASK, _spd) != mode_id)
			continue;

		spd = FIELD_GET(R8C_FANCTL_SPD_MASK, _spd);
	}

	/* 15 is invalid */
	if (spd == 15)
		return -EINVAL;

	memset(cmdbuf, 0, 4);
	cmdbuf[0] = spd + 0x30;

	ret = landisk_r8c_exec_cmd(h->r8c, CMD_FANCTL, cmdbuf, NULL, 0);

	return ret < 0 ? ret : len;
}

static DEVICE_ATTR_RW(fan1_mode);

static struct attribute *r8c_hwmon_common_attrs[] = {
	&dev_attr_fan1_avail_modes.attr,
	&dev_attr_fan1_mode.attr,
	NULL
};

static struct attribute *r8c_hwmon_thld1_attrs[] = {
	&sensor_dev_attr_fan1_temp_threshold1.dev_attr.attr,
	NULL,
};

/*
 * Note: HDL-XR and HDL-XV have 4x (stop/low/mid/high) and
 *       threshold2 is required
 */

static const struct attribute_group r8c_hwmon_common_group = {
	.attrs = r8c_hwmon_common_attrs,
};

static const struct attribute_group r8c_hwmon_spd_temp1_group = {
	.attrs = r8c_hwmon_thld1_attrs,
};

static const struct attribute_group *r8c_hwmon_hdl_a_groups[] = {
	&r8c_hwmon_common_group,
	NULL,
};

static const struct attribute_group *r8c_hwmon_hdl2_a_groups[] = {
	&r8c_hwmon_common_group,
	&r8c_hwmon_spd_temp1_group,
	NULL,
};

/* receive hwmon error event from R8C */
static int r8c_hwmon_event(struct notifier_block *nb,
			   unsigned long evcode, void *data)
{
	struct r8c_hwmon *h = container_of(nb, struct r8c_hwmon, nb);

	switch ((char)evcode) {
	/*
	 * current mode is auto || (manual && not "stop"), but
	 * no rotate of fan detected
	 */
	case R8C_EV_FANERR:
		pr_err("FAN error detected, please check!");
		return NOTIFY_STOP;

	/* current temperature is higher than critical temperature */
	case R8C_EV_TMPERR:
		u32 thld;
		long temp;

		pr_err("Temperature error detected!\n");

		if (r8c_thld_get(h, &thld) || r8c_temp_get(h, &temp))
			return NOTIFY_STOP;

		pr_err("(current: %ld, threshold: %u)\n",
		       temp, FIELD_GET(0xff, thld));

		return NOTIFY_STOP;
	}

	return NOTIFY_DONE;
}

static const struct hwmon_channel_info *r8c_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_CRIT),
	NULL
};

static const struct hwmon_ops r8c_hwmon_ops = {
	.is_visible = r8c_hwmon_is_visible,
	.read = r8c_hwmon_read,
	.write = r8c_hwmon_write,
};

static const struct hwmon_chip_info r8c_hwmon_chip_info = {
	.ops = &r8c_hwmon_ops,
	.info = r8c_hwmon_info,
};

static const struct landisk_model_info model_list[] = {
	{
		.model_id    = ID_HDL_A,
		.attr_grp    = r8c_hwmon_hdl_a_groups,
		.fan_spd_num = 2,
		.fan_spds    = (u8 []){
			FANCTL_MOD(FAN_MOD_AUTO) | 0,
			FANCTL_MOD(FAN_MOD_STOP) | 1,
			FANCTL_MOD(FAN_MOD_MID)  | 2,
		},
	},
	{
		.model_id    = ID_HDL2_A,
		.attr_grp    = r8c_hwmon_hdl2_a_groups,
		.fan_spd_num = 3,
		.fan_spds    = (u8 []){
			FANCTL_MOD(FAN_MOD_AUTO) | 0,
			FANCTL_MOD(FAN_MOD_LOW)  | 1,
			FANCTL_MOD(FAN_MOD_HIGH) | 2,
			FANCTL_MOD(FAN_MOD_STOP) | 3,
		},
	},
};

static int landisk_r8c_hwmon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *hwmon;
	struct r8c_mcu *r8c = dev_get_drvdata(dev->parent);
	struct r8c_hwmon *h;
	u32 model_id = *(u32 *)dev_get_platdata(dev);
	int i, ret;

	pr_info("R8C hwmon driver\n");

	dev_dbg(dev, "got model id %d\n", model_id);

	h = devm_kzalloc(dev, sizeof(*h), GFP_KERNEL);
	if (!h) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(model_list); i++) {
		if (model_id == model_list[i].model_id)
			h->model = &model_list[i];
	}

	if (!h->model) {
		dev_err(dev, "unsupported model\n");
		return -ENOTSUPP;
	}

	dev_set_drvdata(dev, h);

	h->r8c = r8c;
	h->nb.notifier_call = r8c_hwmon_event;
	h->nb.priority = 120;

	ret = devm_landisk_r8c_register_event_notifier(dev, &h->nb);
	if (ret)
		return ret;

	hwmon = devm_hwmon_device_register_with_info(dev, dev_name(dev),
						     h,
						     &r8c_hwmon_chip_info,
						     h->model->attr_grp);

	if (IS_ERR(hwmon))
		return PTR_ERR(hwmon);

	return 0;
}

static const struct of_device_id landisk_r8c_hwmon_ids[] = {
	{ .compatible = "iodata,landisk-r8c-hwmon" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, landisk_r8c_hwmon_ids);

static struct platform_driver landisk_r8c_hwmon_driver = {
	.probe = landisk_r8c_hwmon_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = landisk_r8c_hwmon_ids,
	},
};
module_platform_driver(landisk_r8c_hwmon_driver);

MODULE_DESCRIPTION("R8C MCU hwmon driver for I-O DATA LAN DISK series");
MODULE_LICENSE("GPL v2");
