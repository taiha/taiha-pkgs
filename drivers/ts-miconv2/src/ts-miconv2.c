// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"ts-miconv2"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/bitfield.h>
#include <linux/of_device.h>
#include <linux/tty.h>
#include <linux/mfd/core.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "ts-miconv2.h"

#define MICON_BAUD		38400

#define MICON_CMD_OP_MASK	BIT(7)
#define MICON_CMD_DLEN_MASK	GENMASK(6,0)
#define MICON_CMD_MAX_DLEN	32	/* max. data length */

#define MICON_CMD_BOOT_END	0x3
#define MICON_CMD_SERMOD_CON	0x0f
#define MICON_CMD_PWR_STATE	0x46
#define MICON_CMD_GET_VER	0x83

struct micon_msg {
	u8 op;
	u8 len;
	u8 cmd;
};

struct miconv2 {
	u32 id;
	struct tty_struct *tty;
	struct mutex lock;

	char buf[40];
	size_t cur_len;
	int err;
	struct micon_msg rxmsg;

	struct completion rpl_recv;
};

struct ts_model_config {
	enum ts_model_id id;
	char *machine_compat;
	char *uart_compat;
	char *uart_name;
	char *irq_compat;
	char *irq_name;
	uint32_t irq_num;
	uint32_t irq_type;
};

struct micon_error {
	u8 val;
	int errno;	/* Linux errno */
	char *msg;
	bool cmd;	/* show cmd on error message */
};

static void ts_miconv2_msg_clear(struct micon_msg *msg)
{
	msg->op = 0;
	msg->len = 0;
	msg->cmd = 0;
}

static int ts_miconv2_exec(struct miconv2 *micon,
			   const unsigned char *buf, size_t len,
			   char *rpl_buf, size_t rpl_len)
{
	unsigned long jiffies_left;
	int ret = 0;

	mutex_lock(&micon->lock);
	micon->rpl_recv = COMPLETION_INITIALIZER_ONSTACK(micon->rpl_recv);
	micon->tty->ops->write(micon->tty, buf, len);
	/* tty_wait_until_sent() is too slow for LEDs with "ataN" trigger */
	//tty_wait_until_sent(micon->tty, HZ * 2);

	jiffies_left = wait_for_completion_timeout(&micon->rpl_recv, HZ * 4);
	if (!jiffies_left) {
		pr_err("command timeout\n");
		ret = -ETIMEDOUT;
	}
	if (micon->err)
		ret = micon->err;

	mutex_unlock(&micon->lock);
	return ret;
}

#define MICON_ERR(_v,_c,_m,_cmd)	{.val=_v, .errno=_c, .msg=_m, .cmd=_cmd}

/* ref: "KURO-NAS/X4 マイコン通信仕様書" (KURO-NAS_X4_micon100.pdf) */
static const struct micon_error micon_error_list[] = {
	MICON_ERR(0xf1, -EIO,        "(UART) buffer overrun",             false),
	MICON_ERR(0xf2, -EIO,        "(UART) framing error",              false),
	MICON_ERR(0xf3, -EIO,        "(UART) parity error",               false),
	MICON_ERR(0xf4, -EOPNOTSUPP, "unavailable command",               true),
	MICON_ERR(0xf5, -EINVAL,     "invalid length for command",        true),
	MICON_ERR(0xf6, -EOVERFLOW,  "data is larger than 32 on command", true),
	MICON_ERR(0xf7, -EINVAL,     "invalid checksum on command",       true),
	{ /* sentinel */ }
};

int ts_miconv2_exec_cmd(struct miconv2 *micon, u8 op, u8 cmd, const u8 *data,
			size_t dlen, char *rpl_buf, size_t rpl_len, bool ign_err)
{
	unsigned char *buf;
	u8 cksum = 0;
	int ret, i;

	if (!micon)
		return -EINVAL;

	buf = kzalloc(dlen + 3, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	micon->err = 0;
	ts_miconv2_msg_clear(&micon->rxmsg);
	buf[0] = op | dlen;
	buf[1] = cmd;
	if (dlen > 0)
		memcpy(buf + 2, data, dlen);
	for (i = 0; i < dlen + 2; i++)
		cksum += buf[i];
	cksum = 0 - cksum;
	buf[dlen + 2] = cksum;

//	pr_info("SEND: \"%10phD\"\n", buf);
//	pr_info("SCNT: %u\n", dlen + 3);

	ret = ts_miconv2_exec(micon, buf, dlen + 3, rpl_buf, rpl_len);
	kfree(buf);
	if (ret) {
		//pr_err("%s: err (%d)\n", __func__, ret);
		goto exit;
	}

	/* Error Check */
	if (micon->rxmsg.op  != op ||
	    micon->rxmsg.cmd != cmd) {
		pr_err("this received message is not for current execution\n");
		ret = -EINVAL;
		goto exit;
	}
	if (!ign_err && /* skip checking of MICON error code when specified */
	    (op == MICON_CMD_OP_WR || micon->rxmsg.len == 1)) {
		const struct micon_error *err = &micon_error_list[0];
		/* search error code */
		do {
			if (micon->buf[2] != err->val)
				continue;

			if (err->cmd)
				pr_debug("%s 0x%02x\n", err->msg, cmd);
			else
				pr_debug("%s\n", err->msg);
			ret = err->errno;
			goto exit;
		} while ((++err)->val);
	}

	if (!rpl_len)
		return 0;

	ret = micon->rxmsg.len < rpl_len ? micon->rxmsg.len : rpl_len;
//	pr_info("\"%18phD\" (len: %d)\n", micon->buf + 2, ret);
	memcpy(rpl_buf, &micon->buf[2], ret);

exit:
	return ret;
}
EXPORT_SYMBOL_GPL(ts_miconv2_exec_cmd);

int ts_miconv2_read_unsigned(struct miconv2 *micon, u8 cmd,
			     void *val, int vlen, bool ign_err)
{
	u8 buf[4];
	int ret;

	ret = ts_miconv2_exec_cmd(micon, MICON_CMD_OP_RD, cmd,
				  NULL, 0, buf, vlen, ign_err);
	if (ret != vlen)
		return ret < 0 ? ret : -EINVAL;

	switch (vlen) {
	case 1:
		*(u8 *)val = buf[0];
		break;
	case 2:
		*(u16 *)val  = buf[0] << 8;
		*(u16 *)val |= buf[1];
		break;
	case 4:
		*(u32 *)val  = buf[0] << 24;
		*(u32 *)val |= buf[1] << 16;
		*(u32 *)val |= buf[2] << 8;
		*(u32 *)val |= buf[3];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ts_miconv2_read_unsigned);

int ts_miconv2_write_unsigned(struct miconv2 *micon, u8 cmd, u32 val, int vlen)
{
	u8 buf[4];

	switch (vlen) {
	case 1:
		buf[0] = FIELD_GET(GENMASK(7,0), val);
		break;
	case 2:
		buf[0] = FIELD_GET(GENMASK(15,8), val);
		buf[1] = FIELD_GET(GENMASK(7,0), val);
		break;
	case 4:
		buf[0] = FIELD_GET(GENMASK(31,24), val);
		buf[1] = FIELD_GET(GENMASK(23,16), val);
		buf[2] = FIELD_GET(GENMASK(15,8), val);
		buf[3] = FIELD_GET(GENMASK(7,0), val);
		break;
	default:
		return -EINVAL;
	}

	return ts_miconv2_exec_cmd(micon, MICON_CMD_OP_WR, cmd,
				   buf, vlen, NULL, 0, false);
}
EXPORT_SYMBOL_GPL(ts_miconv2_write_unsigned);

static int ts_miconv2_receive_buf(struct tty_port *port, const unsigned char *cp,
				  const unsigned char *fp, size_t count)
{
	struct miconv2 *micon = port->client_data;

//	pr_info("RECV: \"%18phD\"\n", cp);
//	pr_info("RCNT: %u\n", count);

	/* 
	 * - current length + count is larger than buffer length
	 * - cmd is registered, and current length + count is larger than
	 *   data length + 3(op, cmd, cksum)
	 */
	if (micon->cur_len + count > 40 ||
	    (micon->rxmsg.cmd != 0 &&
	      (micon->cur_len + count) > micon->rxmsg.len + 3)) {
		micon->cur_len = 0;
		micon->err = -EINVAL;
		complete(&micon->rpl_recv);
		return count;
	}
	memcpy(micon->buf + micon->cur_len, cp, count);
	micon->cur_len += count;

	/* current length is larger than 3 and cmd is not registered */
	if (micon->cur_len > 3 && micon->rxmsg.cmd == 0x0) {
		micon->rxmsg.op  = micon->buf[0] & MICON_CMD_OP_MASK;
		micon->rxmsg.len = micon->buf[0] & MICON_CMD_DLEN_MASK;
		micon->rxmsg.cmd = micon->buf[1];
	};

	/*
	 * cmd is registered, and current length is equal to
	 * data len + 3(op,cmd,cksum)
	 */
	if (micon->rxmsg.cmd != 0x0 &&
	    micon->cur_len == micon->rxmsg.len + 3) {
		pr_debug("complete receiving");
		micon->cur_len = 0;
		complete(&micon->rpl_recv);
	}

	pr_debug("MSG: op-> 0x%02x, len-> 0x%02x, cmd-> 0x%02x\n",
		micon->rxmsg.op, micon->rxmsg.len, micon->rxmsg.cmd);
	pr_debug("BUF: \"%40phD\", cur_len: %u\n",
		micon->buf, micon->cur_len);
	return count;
}

static void ts_miconv2_wakeup(struct tty_port *port)
{
	struct tty_struct *tty = tty_port_tty_get(port);

	if (tty) {
		tty_wakeup(tty);
		tty_kref_put(tty);
	}
}

const struct tty_port_client_operations ts_miconv2_client_ops = {
	.receive_buf = ts_miconv2_receive_buf,
	.write_wakeup = ts_miconv2_wakeup,
};

static int ts_miconv2_micon_init(struct miconv2 *micon)
{
	char buf[32];
	int ret;
	u8 val;

	micon->cur_len = 0;
	/*
	 * execute boot_end command to exit boot state of MICON
	 * and start interrupt (GPIO40 on SoC?)
	 */
	ts_miconv2_write_flag(micon, MICON_CMD_BOOT_END);

	msleep(10);
	/* read model/version of MICON */
	ret = ts_miconv2_exec_cmd(micon, MICON_CMD_OP_RD, MICON_CMD_GET_VER,
				  NULL, 0, buf, 32, false);
	if (ret <= 0) {
		pr_err("failed to get model/ver (%d)\n", ret);
		return ret < 0 ? ret : -EINVAL;
	}
	buf[ret] = '\0';
	pr_info("MICON: %s\n", buf);

	/* read power_state (cold start: 2, other: value that set when rebooting) */
	ret = ts_miconv2_read_u8(micon, MICON_CMD_PWR_STATE, &val, false);
	if (ret) {
		pr_err("failed to get power_state\n");
		return ret;
	}
	pr_debug("power state is %u\n", val);

	/*
	 * set serialmode_console
	 * Note: setting serialmode_console mode again will breaks console input
	 */
	switch (val) {
	case MICON_PWR_STAT_CLDBOOT: /* cold boot (first boot on AC) */
	case MICON_PWR_STAT_PWROFF:  /* turned off by normal operations */
	case MICON_PWR_STAT_RUNNING: /* forcibly turned off by Power button */
		pr_info("set serialmode to console\n");
		ts_miconv2_write_flag(micon, MICON_CMD_SERMOD_CON);
		break;
	}

	return 0;
}

static struct mfd_cell ts_miconv2_devs[] = {
	MFD_CELL_OF("leds-ts-miconv2", NULL, NULL, 0, 0,
		    "buffalo,ts-miconv2-leds"),
	MFD_CELL_OF("ts-miconv2-keys", NULL, NULL, 0, 0,
		    "buffalo,ts-miconv2-keys"),
	MFD_CELL_OF("ts-miconv2-hwmon", NULL, NULL, 0, 0,
		    "buffalo,ts-miconv2-hwmon"),
	MFD_CELL_OF("ts-miconv2-beeper", NULL, NULL, 0, 0,
		    "buffalo,ts-miconv2-beeper"),
	MFD_CELL_OF("ts-miconv2-reset", NULL, NULL, 0, 0,
		    "buffalo,ts-miconv2-reset"),
	MFD_CELL_OF("ts-miconv2-wdt", NULL, NULL, 0, 0,
		    "buffalo,ts-miconv2-wdt"),
};

static struct device_node *of_find_availnode_by_compat_fullname(const char *compat,
						    const char *fullname)
{
	struct device_node *np, *_np;

	for_each_compatible_node(_np, NULL, compat) {
		pr_debug("full_name-> \"%s\"\n", of_node_full_name(_np));
		if (of_device_is_available(_np) &&
		    !strcmp(of_node_full_name(_np), fullname))
			np = _np;
	}

	return np ? np : ERR_PTR(-ENODEV);
}

static struct tty_struct *of_tty_kopen_by_compat_fullname(const char *compat,
							  const char *fullname)
{
	struct device_node *np; //, *_np;
	struct device *dev;
	struct tty_struct *tty;
	char buf[8];
	int ret, i;

	np = of_find_availnode_by_compat_fullname(compat, fullname);
	if (IS_ERR(np))
		return (void *)np;

	/* from of_find_device_by_node() in drivers/of/platform.c */
	dev = bus_find_device_by_of_node(&platform_bus_type, np);
	if (!dev)
		return ERR_PTR(-ENODEV);

	for (i = 1; i < 4; i++) {
		dev_t devnm;
		ret = scnprintf(buf, 8, "ttyS%d", i);
		if (ret < 5)
			continue;

		ret = tty_dev_name_to_number(buf, &devnm);
		if (ret)
			continue;
		tty = tty_kopen_exclusive(devnm);
		if (IS_ERR(tty))
			continue;

		pr_debug("%s: tty->dev->parent == dev?: %s\n",
			buf, tty->dev->parent == dev ? "true" : "false");
		if (tty->dev->parent == dev)
			break;

		tty_kclose(tty);
		tty = NULL;
	}

	return tty ? tty : ERR_PTR(-ENODEV);
}

/* based on of_irq_parse_one (drivers/of/irq.c) */
static int ts_miconv2_get_irq_by_model(const struct ts_model_config *model)
{
	struct device_node *np;
	struct of_phandle_args irq_ph;
	struct irq_domain *domain;
	u32 intsize;
	int ret;

	np = of_find_availnode_by_compat_fullname(model->irq_compat,
						  model->irq_name);
	if (IS_ERR(np))
		return PTR_ERR(np);

	if (of_property_read_u32(np, "#interrupt-cells", &intsize)) {
		ret = -EINVAL;
		goto exit;
	}

	irq_ph.np = np;
	irq_ph.args_count = intsize;
	irq_ph.args[0] = model->irq_num;  /* irq number on controller */
	irq_ph.args[1] = model->irq_type; /* IRQ_TYPE_* */

	ret = of_irq_parse_raw(NULL, &irq_ph);
	if (ret)
		return ret;

	/* check irq domain */
	domain = irq_find_host(np);
	if (!domain) {
		ret = -EPROBE_DEFER;
		goto exit;
	}

	ret = irq_create_of_mapping(&irq_ph);
	if (ret < 0)
		goto exit;

exit:
	of_node_put(np);
	return ret;
}

static struct ts_model_config ts_configs[] = {
	{
		.machine_compat = "buffalo,ts3400d",
		.uart_compat = "snps,dw-apb-uart",
		.uart_name = "serial@12300",
		.irq_compat = "marvell,armada-370-gpio",
		.irq_name = "gpio@18140",
		.irq_num = 8,
		.irq_type = 2,
	},
	{
		.machine_compat = "buffalo,ts3400d-hdd",
		.uart_compat = "snps,dw-apb-uart",
		.uart_name = "serial@12300",
		.irq_compat = "marvell,armada-370-gpio",
		.irq_name = "gpio@18140",
		.irq_num = 8,
		.irq_type = 2,
	},
	{
		.machine_compat = "buffalo,ts3400d-usb",
		.uart_compat = "snps,dw-apb-uart",
		.uart_name = "serial@12300",
		.irq_compat = "marvell,armada-370-gpio",
		.irq_name = "gpio@18140",
		.irq_num = 8,
		.irq_type = 2,
	},
};

static int ts_miconv2_plat_probe(struct platform_device *pdev)
{
	struct micon_child_pdata *child_pdata;
	struct ts_model_config *model;
	struct tty_struct *tty;
	struct ktermios ktermios;
	struct miconv2 *micon;
	int ret, i;

	pr_info("MICON v2 MCU driver for Buffalo TeraStation series\n");

	for (i = 0; i < ARRAY_SIZE(ts_configs); i++) {
		if (of_machine_is_compatible(ts_configs[i].machine_compat)) {
			model = &ts_configs[i];
			break;
		}
	}
	if (!model) {
		dev_err(&pdev->dev, "this TeraStation is not supported\n");
		return -ENOTSUPP;
	}

	tty = of_tty_kopen_by_compat_fullname(model->uart_compat, model->uart_name);
	if (IS_ERR(tty)) {
		dev_err(&pdev->dev, "failed to get tty of %s (%ld)\n",
			model->uart_name, PTR_ERR(tty));
	}

	/* check ops */
	if (!tty->ops->open ||
	    !tty->ops->close ||
	    !tty->ops->write) {
		dev_err(&pdev->dev, "tty %s doesn't support required op(s)\n",
			tty_name(tty));
		ret = -ENODEV;
		goto err_unlock;
	}

	/* check port availability */
	if (!tty->port) {
		dev_err(&pdev->dev, "tty port of %s is unavailable\n",
			tty_name(tty));
		ret = -ENODEV;
		goto err_unlock;
	}

	ret = tty->ops->open(tty, NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to open tty %s (%d)\n",
			tty_name(tty), ret);
		goto err_unlock;
	}

	dev_dbg(&pdev->dev, "got tty %s\n", tty_name(tty));

	/*
	 * Setup tty
	 * 38400bps, parity even, no flow control
	 */
	ktermios = tty->termios;
	ktermios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
			      INLCR | IGNCR | ICRNL | IXON);
	ktermios.c_oflag &= ~OPOST;
	ktermios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	ktermios.c_cflag &= ~(CSIZE | PARENB | HUPCL | CMSPAR | CRTSCTS);
	ktermios.c_cflag |= CS8;
	ktermios.c_cflag |= PARENB;
	/* Hangups are not supported so make sure to ignore carrier detect. */
	ktermios.c_cflag |= CLOCAL;
	tty_termios_encode_baud_rate(&ktermios, MICON_BAUD, MICON_BAUD);
	tty_set_termios(tty, &ktermios);

	micon = devm_kzalloc(&pdev->dev, sizeof(*micon), GFP_KERNEL);
	if (!micon) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_close;
	}
	micon->tty = tty;
	platform_set_drvdata(pdev, micon);

	/* set client ops */
	mutex_lock(&tty->port->mutex);
	tty->port->client_ops = &ts_miconv2_client_ops;
	tty->port->client_data = micon;
	mutex_unlock(&tty->port->mutex);

	mutex_init(&micon->lock);

	tty_perform_flush(micon->tty, TCIOFLUSH);
	ret = ts_miconv2_micon_init(micon);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize MCU\n");
		goto err_reset_ops;
	}

	/* setup platform_data for child devices */
	child_pdata = devm_kzalloc(&pdev->dev, sizeof(*child_pdata), GFP_KERNEL);
	if (!child_pdata) {
		ret = -ENOMEM;
		goto err_reset_ops;
	}
	for (i = 0; i < ARRAY_SIZE(ts_miconv2_devs); i++) {
		ts_miconv2_devs[i].platform_data = child_pdata;
		ts_miconv2_devs[i].pdata_size = sizeof(*child_pdata);
	}

	/* get irq number */
	ret = ts_miconv2_get_irq_by_model(model);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get irq\n");
		goto err_reset_ops;
	}
	dev_info(&pdev->dev, "got irq %d\n", ret);

	child_pdata->id = model->id;
	child_pdata->irq = ret;

	ret = devm_mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
				   ts_miconv2_devs,
				   ARRAY_SIZE(ts_miconv2_devs),
				   NULL, 0, NULL);
	if (!ret)
		return 0;

	dev_err(&pdev->dev, "failed to add MFD devices\n");

err_reset_ops:
	mutex_lock(&tty->port->mutex);
	tty->port->client_ops = &tty_port_default_client_ops;
	tty->port->client_data = NULL;
	mutex_unlock(&tty->port->mutex);

err_close:
	tty->ops->close(tty, NULL);

err_unlock:
	tty_unlock(tty);
	tty_kclose(tty);

	return ret;
}

static int ts_miconv2_plat_remove(struct platform_device *pdev)
{
	struct miconv2 *micon = platform_get_drvdata(pdev);
	struct tty_struct *tty = micon->tty;

	mutex_lock(&tty->port->mutex);
	tty->port->client_ops = &tty_port_default_client_ops;
	tty->port->client_data = NULL;
	mutex_lock(&tty->port->mutex);

	tty_lock(tty);
	tty->ops->close(tty, NULL);
	tty_unlock(tty);
	tty_kclose(tty);

	return 0;
}

static struct platform_driver ts_miconv2_platdriver = {
	.probe	= ts_miconv2_plat_probe,
	.remove = ts_miconv2_plat_remove,
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

struct platform_device ts_miconv2_platdev = {
	.name	= DRV_NAME,
	.id	= -1,
};

static int __init ts_miconv2_init(void)
{
	platform_device_register(&ts_miconv2_platdev);

	return platform_driver_register(&ts_miconv2_platdriver);
}

static void __exit ts_miconv2_exit(void)
{
	platform_driver_unregister(&ts_miconv2_platdriver);
}

module_init(ts_miconv2_init);
module_exit(ts_miconv2_exit);

MODULE_AUTHOR("INAGAKI Hiroshi <musashino.open@gmail.com>");
MODULE_DESCRIPTION("MICON v2 MCU driver for Buffalo TeraStation series");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
