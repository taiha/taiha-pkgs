// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef _LANDISK_R8C_H_
#define _LANDISK_R8C_H_

#include <linux/bits.h>
#include <linux/notifier.h>

enum id {
	ID_HDL_A = 0,
	ID_HDL2_A,
	ID_HDL_MAX = ID_HDL2_A,
};

struct r8c_mcu;

int landisk_r8c_exec_cmd(struct r8c_mcu *r8c, const char *cmd, const char *arg,
		 char *rcv_buf, size_t rcv_len);
int devm_landisk_r8c_register_event_notifier(struct device *dev,
					     struct notifier_block *nb);

#endif /* _LANDISK_R8C_ */
