// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef _TS_MICONV2_H_
#define _TS_MICONV2_H_

#include <linux/bits.h>
#include <linux/notifier.h>

struct miconv2;

enum ts_model_id {
	ID_TS3400D = 0,
};

struct micon_child_pdata {
	enum ts_model_id id;
	int irq;
};

#define MICON_CMD_OP_RD		0x80
#define MICON_CMD_OP_WR		0x00

#define MICON_PWR_STAT_CLDBOOT	2
#define MICON_PWR_STAT_RUNNING	3
#define MICON_PWR_STAT_RESTART	4
#define MICON_PWR_STAT_PWROFF	5

int ts_miconv2_exec_cmd(struct miconv2 *, u8, u8, const u8 *,
			size_t, char *, size_t, bool);

int ts_miconv2_read_unsigned(struct miconv2 *, u8, void *, int, bool);
int ts_miconv2_write_unsigned(struct miconv2 *, u8, u32, int);

#define ts_miconv2_read_u8(_m,_c,_v,_e)	\
	ts_miconv2_read_unsigned(_m,_c,(void *)_v,sizeof(u8),_e);
#define ts_miconv2_read_u16(_m,_c,_v)	\
	ts_miconv2_read_unsigned(_m,_c,(void *)_v,sizeof(u16),false);
#define ts_miconv2_read_u32(_m,_c,_v)	\
	ts_miconv2_read_unsigned(_m,_c,(void *)_v,sizeof(u32),false);

#define ts_miconv2_write_u8(_m,_c,_v)	\
	ts_miconv2_write_unsigned(_m,_c,_v,sizeof(u8));
#define ts_miconv2_write_u16(_m,_c,_v)	\
	ts_miconv2_write_unsigned(_m,_c,_v,sizeof(u16));
#define ts_miconv2_write_u32(_m,_c,_v)	\
	ts_miconv2_write_unsigned(_m,_c,_v,sizeof(u32));

#define ts_miconv2_write_flag(_m,_c)	\
	ts_miconv2_exec_cmd(_m,MICON_CMD_OP_WR,_c,NULL,0,NULL,0,false);

#endif /* _TS_MICONV2_ */
