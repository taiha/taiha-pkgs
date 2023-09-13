// SPDX-License-Identifier: GPL-2.0-or-later

#define DRV_NAME	"landisk-r8c"
#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/bitfield.h>
#include <linux/of_device.h>
#include <linux/tty.h>
#include <linux/mfd/core.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "landisk-r8c.h"

#define R8C_TTY			"ttyS1"
#define R8C_BAUD		57600

#define R8C_MSG_MIN_LEN		2	/* "@<key code>" */
#define R8C_MSG_MAX_LEN		64	/* "":<cmd> <params>\n" */

#define R8C_MSG_TYPE_CMD	':'
#define R8C_MSG_TYPE_REPL	';'
#define R8C_MSG_TYPE_EV		'@'

struct r8c_mcu {
	u32 id;
	struct tty_struct *tty;
	struct mutex lock;

	char ev_code;
	char buf[32];

	struct delayed_work ev_work;
	struct blocking_notifier_head ev_list;
	struct completion rpl_recv;
};

static void landisk_r8c_unregister_event_notifier(struct device *dev,
						  void *res)
{
	struct r8c_mcu *r8c = dev_get_drvdata(dev->parent);
	struct notifier_block *nb = *(struct notifier_block **)res;
	struct blocking_notifier_head *bnh = &r8c->ev_list;

	WARN_ON(blocking_notifier_chain_unregister(bnh, nb));
}

int devm_landisk_r8c_register_event_notifier(struct device *dev,
					     struct notifier_block *nb)
{
	struct r8c_mcu *r8c = dev_get_drvdata(dev->parent);
	struct notifier_block **rcnb;
	int ret;

	rcnb = devres_alloc(landisk_r8c_unregister_event_notifier,
			    sizeof(*rcnb), GFP_KERNEL);
	if (!rcnb)
		return -ENOMEM;

	ret = blocking_notifier_chain_register(&r8c->ev_list, nb);
	if (!ret) {
		*rcnb = nb;
		devres_add(dev, rcnb);
	} else {
		devres_free(rcnb);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_landisk_r8c_register_event_notifier);

static void landisk_r8c_notifier_call_work(struct work_struct *work)
{
	struct r8c_mcu *r8c = container_of(work, struct r8c_mcu,
					   ev_work.work);

	blocking_notifier_call_chain(&r8c->ev_list, r8c->ev_code, NULL);
}

static void landisk_r8c_clear_buf(char *buf)
{
	memset(buf, 0, 32);
}

static int landisk_r8c_exec(struct r8c_mcu *r8c, const char *buf, size_t len,
			    char *rpl_buf, size_t rpl_len)
{
	unsigned long jiffies_left;
	int ret = 0;

	if (len > R8C_MSG_MAX_LEN)
		return -EINVAL;

	pr_debug("buf-> \"%s\" (%d bytes)\n", buf, len);

	mutex_lock(&r8c->lock);
	r8c->rpl_recv = COMPLETION_INITIALIZER_ONSTACK(r8c->rpl_recv);
	r8c->tty->ops->write(r8c->tty, buf, len);
	/* tty_wait_until_sent() is too slow for LEDs with "ataN" trigger */
	//tty_wait_until_sent(r8c->tty, HZ);

	if (rpl_len <= 0)
		goto exit;

	jiffies_left = wait_for_completion_timeout(&r8c->rpl_recv, HZ);
	if (!jiffies_left) {
		pr_err("command timeout\n");
		ret = -ETIMEDOUT;
		goto exit_clear;
	}
	ret = scnprintf(rpl_buf, rpl_len, "%s", r8c->buf);

exit_clear:
	landisk_r8c_clear_buf(r8c->buf);

exit:
	mutex_unlock(&r8c->lock);
	return ret;
}

int landisk_r8c_exec_cmd(struct r8c_mcu *r8c, const char *cmd, const char *arg,
			 char *rpl_buf, size_t rpl_len)
{
	char buf[R8C_MSG_MAX_LEN];
	int ret;

	if (!r8c)
		return -EINVAL;

	/* don't place a space at the end when no argument specified */
	ret = scnprintf(buf, R8C_MSG_MAX_LEN, "%c%s%s%s\n",
			R8C_MSG_TYPE_CMD, cmd, arg ? " " : "", arg ? arg : "");
	return landisk_r8c_exec(r8c, buf, ret, rpl_buf, rpl_len);
}
EXPORT_SYMBOL_GPL(landisk_r8c_exec_cmd);

static ssize_t command_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct r8c_mcu *r8c = dev_get_drvdata(dev);
	ssize_t ret;
	char cmdbuf[R8C_MSG_MAX_LEN];
	char rplbuf[R8C_MSG_MAX_LEN];
	const char *cur = buf;

	if (len == 0 || len > R8C_MSG_MAX_LEN - 2)
		return -EINVAL;

	if (cur[0] == R8C_MSG_TYPE_CMD) {
		if (len <= 1)
			return -EINVAL;
		cur++;
	}

	ret = scnprintf(cmdbuf, R8C_MSG_MAX_LEN, ":%s%s",
			cur, strchr(buf, '\n') ? "" : "\n");
	pr_debug("COMMAND: \"%s\"\n", cmdbuf);
	ret = landisk_r8c_exec(r8c, cmdbuf, ret, rplbuf, R8C_MSG_MAX_LEN);
	if (ret <= 0)
		return ret;

	pr_info("RESULT: \"%s\"\n", rplbuf);

	return len;
}

static DEVICE_ATTR_WO(command);

static int landisk_r8c_receive_buf(struct tty_port *port, const unsigned char *cp,
				   const unsigned char *fp, size_t count)
{
	struct r8c_mcu *r8c = port->client_data;
	int ret;

//	pr_info("RECEIVE: \"%s\"\n", cp);
//	pr_info("COUNT: %u\n", count);

	switch (cp[0]) {
	case R8C_MSG_TYPE_EV:
		pr_debug("ev code: %c\n", cp[1]);
		r8c->ev_code = cp[1];
		mod_delayed_work(system_wq, &r8c->ev_work, 0);
		break;
	case R8C_MSG_TYPE_REPL:
		char *lf;
		ret = scnprintf(r8c->buf, 32, "%s", cp + 1);
		lf = strchr(r8c->buf, '\n');
		if (lf)
			lf[0] = '\0';
//		pr_info("buf: \"%s\"\n", r8c->buf);
		complete(&r8c->rpl_recv);
		break;
	}

	return count;
}

static void landisk_r8c_wakeup(struct tty_port *port)
{
	struct tty_struct *tty = tty_port_tty_get(port);

	if (tty) {
		tty_wakeup(tty);
		tty_kref_put(tty);
	}
}

const struct tty_port_client_operations landisk_r8c_client_ops = {
	.receive_buf = landisk_r8c_receive_buf,
	.write_wakeup = landisk_r8c_wakeup,
};

static const char *landisk_r8c_models[] = {
	[ID_HDL_A]  = "HDL-A",
	[ID_HDL2_A] = "HDL2-A",
};

static int landisk_r8c_detect(struct r8c_mcu *r8c)
{
	char model[12], ver[8];
	int ret, i;

	ret = landisk_r8c_exec_cmd(r8c, "model", NULL, model, 12);
	if (ret > 0)
		ret = landisk_r8c_exec_cmd(r8c, "ver", NULL, ver, 8);
	if (ret <= 0)
		return ret == 0 ? -EINVAL : ret;

	pr_debug("ORIG: %s, %s\n", model, ver);
	for (i = 0; i < ARRAY_SIZE(landisk_r8c_models); i++) {
		if (!strncmp(model, landisk_r8c_models[i],
			     strlen(landisk_r8c_models[i]))) {
			r8c->id = i;
			pr_info("MCU: %s v%s\n", landisk_r8c_models[i], ver);
			return 0;
		}
	}

	return -ENOTSUPP;
}

static struct mfd_cell landisk_r8c_devs[] = {
	MFD_CELL_OF("leds-landisk-r8c", NULL, NULL, 0, 0,
		    "iodata,landisk-r8c-leds"),
	MFD_CELL_OF("landisk-r8c-keys", NULL, NULL, 0, 0,
		    "iodata,landisk-r8c-keys"),
	MFD_CELL_OF("landisk-r8c-hwmon", NULL, NULL, 0, 0,
		    "iodata,landisk-r8c-hwmon"),
	MFD_CELL_OF("landisk-r8c-beeper", NULL, NULL, 0, 0,
		    "iodata,landisk-r8c-beeper"),
	MFD_CELL_OF("landisk-r8c-reset", NULL, NULL, 0, 0,
		    "iodata,landisk-r8c-reset"),
};

static int landisk_r8c_plat_probe(struct platform_device *pdev)
{
	struct tty_struct *tty;
	struct ktermios ktermios;
	struct r8c_mcu *r8c;
	dev_t devnm;
	int ret, i;

	pr_info("R8C MCU driver for I-O DATA LAN DISK series\n");

	ret = tty_dev_name_to_number(R8C_TTY, &devnm);
	if (ret) {
		dev_err(&pdev->dev, "failed to get dev_t of tty %s\n (%d)\n",
			R8C_TTY, devnm);
		return ret;
	}

	tty = tty_kopen_exclusive(devnm);
	if (IS_ERR(tty)) {
		dev_err(&pdev->dev, "failed to get tty %s (%ld)\n",
			R8C_TTY, PTR_ERR(tty));
	}

	/* check ops */
	if (!tty->ops->open ||
	    !tty->ops->close ||
	    !tty->ops->write) {
		dev_err(&pdev->dev, "tty %s doesn't support required op(s)\n",
			R8C_TTY);
		ret = -ENODEV;
		goto err_unlock;
	}

	/* check port availability */
	if (!tty->port) {
		dev_err(&pdev->dev, "tty port of %s is unavailable\n", R8C_TTY);
		ret = -ENODEV;
		goto err_unlock;
	}

	ret = tty->ops->open(tty, NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to open tty %s (%d)\n",
			R8C_TTY, ret);
		goto err_unlock;
	}

	dev_dbg(&pdev->dev, "got tty %s\n", R8C_TTY);

	/*
	 * Setup tty
	 * 57600bps, no parity, no flow control
	 */
	ktermios = tty->termios;
	ktermios.c_cflag &= ~(CBAUD | CRTSCTS | PARENB | PARODD );
	tty_termios_encode_baud_rate(&ktermios, R8C_BAUD, R8C_BAUD);
	tty_set_termios(tty, &ktermios);

	r8c = devm_kzalloc(&pdev->dev, sizeof(*r8c), GFP_KERNEL);
	if (!r8c) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_close;
	}
	r8c->tty = tty;
	platform_set_drvdata(pdev, r8c);

	/* set client ops */
	mutex_lock(&tty->port->mutex);
	tty->port->client_ops = &landisk_r8c_client_ops;
	tty->port->client_data = r8c;
	mutex_unlock(&tty->port->mutex);

	mutex_init(&r8c->lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&r8c->ev_list);
	INIT_DELAYED_WORK(&r8c->ev_work, landisk_r8c_notifier_call_work);

	//tty_perform_flush(r8c->tty, TCOFLUSH);
	ret = landisk_r8c_detect(r8c);
	if (ret) {
		pr_err("this MCU is not supported\n");
		goto err_reset_ops;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_command);
	if (ret) {
		dev_err(&pdev->dev, "failed to create command sysfs\n");
		goto err_reset_ops;
	}

	for (i = 0; i < ARRAY_SIZE(landisk_r8c_devs); i++) {
		landisk_r8c_devs[i].platform_data = &r8c->id;
		landisk_r8c_devs[i].pdata_size = sizeof(u32);
	}
	ret = devm_mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
				   landisk_r8c_devs,
				   ARRAY_SIZE(landisk_r8c_devs),
				   NULL, 0, NULL);
	if (ret) {
		pr_err("failed to add MFD devices\n");
		goto err_remove_devfile;
	}

	return 0;

err_remove_devfile:
	device_remove_file(&pdev->dev, &dev_attr_command);

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

static int landisk_r8c_plat_remove(struct platform_device *pdev)
{
	struct r8c_mcu *r8c = platform_get_drvdata(pdev);
	struct tty_struct *tty = r8c->tty;

	device_remove_file(&pdev->dev, &dev_attr_command);

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

static struct platform_driver landisk_r8c_platdriver = {
	.probe	= landisk_r8c_plat_probe,
	.remove = landisk_r8c_plat_remove,
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

struct platform_device landisk_r8c_platdev = {
	.name	= DRV_NAME,
	.id	= -1,
};

static int __init landisk_r8c_init(void)
{
	platform_device_register(&landisk_r8c_platdev);

	return platform_driver_register(&landisk_r8c_platdriver);
}

static void __exit landisk_r8c_exit(void)
{
	platform_driver_unregister(&landisk_r8c_platdriver);
}

module_init(landisk_r8c_init);
module_exit(landisk_r8c_exit);

MODULE_AUTHOR("INAGAKI Hiroshi <musashino.open@gmail.com>");
MODULE_DESCRIPTION("R8C MCU driver for I-O DATA LAN DISK series NAS");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
