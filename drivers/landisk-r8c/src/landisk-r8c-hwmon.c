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
 * fancont setting
 *
 * return val by fancont command
 *
 *   ex.: 12
 *        1--> Manual (0-> Auto)
 *         2-> Speed 2
 */
#define R8C_FANCTL_MOD_MASK	0xf0U
#define R8C_FANCTL_SPD_MASK	0xfU
#define R8C_FANCTL_MOD_AUTO	"0"

#define R8C_FANMAP(_st,_pa) 	(_st << 4 | _pa)
#define R8C_FANMAP_ST_MASK	0xf0U
#define R8C_FANMAP_PA_MASK	0xfU

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

/* others */
#define R8C_FANLABEL		"Rear FAN"
#define R8C_FANLABEL_MAX	44

struct landisk_model_info {
	u32 model_id;
	const struct attribute_group **attr_grp;
	u8 fan_spd_num;
	const u8 *fan_spds;
};

struct r8c_hwmon {
	struct r8c_mcu *r8c;
	struct notifier_block nb;
	char fan_label[R8C_FANLABEL_MAX];
	const struct landisk_model_info *model;
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
	if (ret != 1 || retbuf[0] == '1') {
		pr_err("failed to set threshold\n");
		return ret < 0 ? ret : -EINVAL;
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
		return ret < 0 ? ret : -EINVAL;
	}

	ret = sscanf(retbuf, "%02x %02x %02x", &spd2, &spd1, &terr);
	if (ret != 3)
		return -EINVAL;

	*val = FIELD_PREP(TH_IDX_MASK(R8C_THLD_FANSPD_2ND), spd2) |
	       FIELD_PREP(TH_IDX_MASK(R8C_THLD_FANSPD_1ST), spd1) |
	       FIELD_PREP(TH_IDX_MASK(R8C_THLD_TEMP_CRIT), terr);

	return 0;
}

/* return: millicelsius */
static int r8c_temp_get(struct r8c_hwmon *h, long *temp)
{
	char retbuf[4];
	int ret;

	ret = landisk_r8c_exec_cmd(h->r8c, CMD_TEMP, NULL, retbuf, 4);
	if (ret != 2) {
		pr_err("failed to get temperature\n");
		return ret < 0 ? ret : -EINVAL;
	}

	ret = kstrtol(retbuf, 16, temp);
	if (ret)
		return ret;
	*temp *= 1000;

	return 0;
}

static int r8c_fanctl_get(struct r8c_hwmon *h, long *val, long mask)
{
	char buf[4];
	int ret;

	ret = landisk_r8c_exec_cmd(h->r8c, CMD_FANCTL, NULL, buf, 4);
	if (ret != 2)
		return ret < 0 ? ret : -EINVAL;
	ret = kstrtol(buf, 16, val);
	if (ret)
		return ret;
	if (mask)
		*val = (*val & mask) >> find_first_bit(&mask, BITS_PER_BYTE);
	return 0;
}

static int r8c_hwmon_read_string(struct device *dev, enum hwmon_sensor_types type,
				 u32 attr, int chan, const char **str)
{
	struct r8c_hwmon *h = dev_get_drvdata(dev);

	if (type != hwmon_fan ||
	    attr != hwmon_fan_label)
		return -EINVAL;

	*str = h->fan_label;
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

			*val = FIELD_GET(TH_IDX_MASK(R8C_THLD_TEMP_CRIT), thld);
			*val *= 1000;
			return 0;
		}
		break;
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_enable:
			return r8c_fanctl_get(h, val, R8C_FANCTL_MOD_MASK);
		case hwmon_fan_target:
			u8 fan_spd;
			long param;
			int i;

			ret = r8c_fanctl_get(h, &param, R8C_FANCTL_SPD_MASK);
			if (ret)
				return ret;
			for (i = 0; i < h->model->fan_spd_num; i++) {
				fan_spd = h->model->fan_spds[i];
				if (param != FIELD_GET(R8C_FANMAP_PA_MASK, fan_spd))
					continue;
				*val = i;
				return 0;
			}
			return -EINVAL;
		default:
			break;
		}
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
	int ret;

	if (val < 0)
		return -EINVAL;

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_crit:
			return r8c_thld_set(h, R8C_THLD_TEMP_CRIT, val);
		default:
			break;
		}
		break;
	case hwmon_fan:
		char buf[4];

		/*
		 * buf[0]: Auto(0)/Manual(1)
		 * buf[1]: Current Speed
		 */
		ret = landisk_r8c_exec_cmd(h->r8c, CMD_FANCTL, NULL, buf, 4);
		if (ret != 2)
			return ret < 0 ? ret : -EINVAL;

		switch (attr) {
		case hwmon_fan_enable:
			/* Auto: "0" (0x30), Manual: "1" (0x31) */
			if (!!(val) == !!(buf[0] & GENMASK(3,0)))
				return 0;
			/*
			 * Set speed
			 * - val=0 (disable): set Auto
			 * - val>0 (enable) : set current speed in Auto
			 */
			return landisk_r8c_exec_cmd(h->r8c, CMD_FANCTL,
						    !!(val) ?
							&buf[1] :
							R8C_FANCTL_MOD_AUTO,
						    NULL, 0);
		case hwmon_fan_target:
			const struct landisk_model_info *model = h->model;

			/* return error if not enabled */
			if (!(buf[0] & GENMASK(3,0)))
				return -EINVAL;

			/* val is larger than the max. step */
			if (val > model->fan_spd_num - 1)
				return -EINVAL;

			scnprintf(buf, 4, "%u",
				  FIELD_GET(R8C_FANMAP_PA_MASK,
					    model->fan_spds[val]));
			return landisk_r8c_exec_cmd(h->r8c, CMD_FANCTL,
						    buf, NULL, 0);
		default:
			break;
		}
	default:
		break;
	}

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
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_enable:
		case hwmon_fan_target:
			return 0644;
		case hwmon_fan_label:
			return 0444;
		default:
			break;
		}
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
	u32 thld, val;
	ssize_t ret;

	/* millicelsius */
	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return -EINVAL;
	/* to celsius */
	val /= 1000;

	/* limitation: 1st_threshold < 2nd_threshold */
	ret = r8c_thld_get(h, &thld);
	if (ret)
		return -EINVAL;

	switch (index) {
	case R8C_THLD_FANSPD_2ND:
		/* equal or lower than index 1 (THLD_FANSPD_1ST) */
		if (val <= FIELD_GET(TH_IDX_MASK(R8C_THLD_FANSPD_1ST), thld)) {
			dev_err(dev,
				"please specify higher value than threashold1\n");
			return -EINVAL;
		}
		break;
	case R8C_THLD_FANSPD_1ST:
		/* equal or higher than index 0 (THLD_FANSPD_2ND) */
		if (val >= FIELD_GET(TH_IDX_MASK(R8C_THLD_FANSPD_2ND), thld)) {
			if (h->model->fan_spd_num <= 3) {
				/* set inactive 2nd threshold */
				ret = r8c_thld_set(h, R8C_THLD_FANSPD_2ND,
					   val * 1000 + 1000);
				if (ret)
					return -EINVAL;
			} else {
				dev_err(dev,
					"please specify lower value"
					"than threshold2\n");
				return -EINVAL;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	ret = r8c_thld_set(h, index, val * 1000);

	return ret ? ret : len;
}

/* low->middle or low->high */
static SENSOR_DEVICE_ATTR_RW(fan1_temp_threshold1, fan1_temp_threshold, 1);
/* middle->high */
static SENSOR_DEVICE_ATTR_RW(fan1_temp_threshold2, fan1_temp_threshold, 0);

static struct attribute *r8c_hwmon_thld_attrs[] = {
	&sensor_dev_attr_fan1_temp_threshold1.dev_attr.attr,
	&sensor_dev_attr_fan1_temp_threshold2.dev_attr.attr,
	NULL,
};

/* switch visibility by fan_spd_num of landisk model */
static umode_t r8c_hwmon_thld_visible(struct kobject *kobj,
				      struct attribute *attr, int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct r8c_hwmon *h = dev_get_drvdata(dev);

	switch (h->model->fan_spd_num) {
	case 4:
		return 0644;
	case 3:
		if (attr == &sensor_dev_attr_fan1_temp_threshold1.dev_attr.attr)
			return 0644;
		fallthrough;
	case 2:
	default:
		return 0000;
	}
}

static const struct attribute_group r8c_hwmon_thld_group = {
	.attrs = r8c_hwmon_thld_attrs,
	.is_visible = r8c_hwmon_thld_visible,
};

static const struct attribute_group *r8c_hwmon_thld_groups[] = {
	&r8c_hwmon_thld_group,
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
		       temp / 1000,
		       FIELD_GET(TH_IDX_MASK(R8C_THLD_TEMP_CRIT), thld));

		return NOTIFY_STOP;
	}

	return NOTIFY_DONE;
}

static const struct hwmon_channel_info *r8c_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_CRIT),
	HWMON_CHANNEL_INFO(fan,  HWMON_F_ENABLE | HWMON_F_LABEL | HWMON_F_TARGET),
	NULL
};

static const struct hwmon_ops r8c_hwmon_ops = {
	.is_visible = r8c_hwmon_is_visible,
	.read = r8c_hwmon_read,
	.read_string = r8c_hwmon_read_string,
	.write = r8c_hwmon_write,
};

static const struct hwmon_chip_info r8c_hwmon_chip_info = {
	.ops = &r8c_hwmon_ops,
	.info = r8c_hwmon_info,
};

static const char *fan_mode_list[] = {
	[FAN_MOD_STOP] = "stop",
	[FAN_MOD_LOW]  = "low",
	[FAN_MOD_MID]  = "middle",
	[FAN_MOD_HIGH] = "high",
	[FAN_MOD_AUTO] = "auto",
};

static const struct landisk_model_info model_list[] = {
	{
		.model_id    = ID_HDL_A,
		.fan_spd_num = 2,
		.fan_spds   = (u8 []){
			R8C_FANMAP(FAN_MOD_STOP, 1),
			R8C_FANMAP(FAN_MOD_MID,  2),
		},
	},
	{
		.model_id    = ID_HDL2_A,
		.fan_spd_num = 3,
		.fan_spds   = (u8 []){
			R8C_FANMAP(FAN_MOD_STOP, 3),
			R8C_FANMAP(FAN_MOD_LOW,  1),
			R8C_FANMAP(FAN_MOD_HIGH, 2),
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
	int i, ret, spd;

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

	/* setup fan_label */
	ret = scnprintf(h->fan_label, R8C_FANLABEL_MAX, R8C_FANLABEL " (");
	for (i = 0; i < h->model->fan_spd_num; i++) {
		spd = FIELD_GET(R8C_FANMAP_ST_MASK, h->model->fan_spds[i]);
		if (i > 0)
			ret += scnprintf(h->fan_label + ret,
					 R8C_FANLABEL_MAX - ret,
					 ", ");
		ret += scnprintf(h->fan_label + ret,
				 R8C_FANLABEL_MAX - ret,
				 "%d:%s",
				 i, fan_mode_list[spd]);
	}
	scnprintf(h->fan_label + ret, 44 - ret, ")");

	h->r8c = r8c;
	h->nb.notifier_call = r8c_hwmon_event;
	h->nb.priority = 120;

	ret = devm_landisk_r8c_register_event_notifier(dev, &h->nb);
	if (ret)
		return ret;

	hwmon = devm_hwmon_device_register_with_info(dev, "landisk_r8c_hwmon",
						     h,
						     &r8c_hwmon_chip_info,
						     r8c_hwmon_thld_groups);

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
