/* 
 * based on ubus.c in br101/pingcheck
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>			/* for unix time */
#include <limits.h>			/* for ULONG_MAX */
#include <endian.h>			/* for __BYTE_ORDER */
#include <sys/stat.h>

#include <sys/utsname.h>		/* for uname()*/
#include <libubus.h>
#include <libubox/blobmsg_json.h>

#include "ma-tools.h"
#include "agent_info.h"

static struct ubus_context *ctx;
static struct blob_attr *result_msg;
static struct blob_buf output_buf;		/* for printing to stdout */
static struct blob_buf send_buf;		/* for sending message to call*/
static struct blob_buf load_buf;		/* for loading json string */
static struct blob_buf tmp_buf;		/* for storing temporary sysinfo */

static char agent_name[32];
static char agent_ver[32];
static char hostid[12] = "testid";
static char *jsonpath;
static bool formatted = false;
static bool use_model = false;
static uint32_t timeout = 5;

/*
 * convert l3 device name for metric data
 * ex:
 *   eth0.2 -> eth0_2
 */
static char *cnv_devname(char *str)
{
	int i;

	for (i = 0; i < strlen(str); i++) {
		if (*(str + i) == '.')
			str[i] = '_';
	}

	return str;
}

static uint64_t cnv_xxb(char *str)
{
	uint64_t tmp;
	char *eptr;

	if (strlen(str) < 0)
		return 0;
	
	tmp = strtoull(str, &eptr, 10);
	if (strlen(eptr) > 0 || tmp == ULLONG_MAX)
		return 0;

	return tmp;
}

static void
ubus_receive_result_cb(struct ubus_request *req, int type,
					struct blob_attr *msg)
{
	if(!msg)
		return;

	result_msg = blob_memdup(msg);
}

static int
ubus_lookup_call(char *path_str, const char *method, struct blob_attr *attr)
{
	uint32_t id;
	int ret;

	ret = ubus_lookup_id(ctx, path_str, &id);
	if (ret)
		return ret;
			
	return ubus_invoke(ctx, id, method, attr, ubus_receive_result_cb,
				NULL, timeout * 1000);
}

static int get_l3dev_status(char *devname)
{
	char msg_json[32];

	sprintf(msg_json, "{ \"name\": \"%s\" }", devname);
//	fprintf(stderr, "msg_json: %s\n", msg_json);
	blob_buf_init(&send_buf, 0);
	if (!blobmsg_add_json_from_string(&send_buf, msg_json)) {
		fprintf(stderr, "failed to parse message json\n");
		return -1;
	}
	return ubus_lookup_call("network.device", "status",
				devname ? send_buf.head : NULL);
}

static int print_sysinfo_json(void)
{
	int ret;
	void *tbl, *tbl2, *ary, *ary2;

	ret = ubus_lookup_call("system", "board", NULL);
	if (ret)
		return ret;
	
	struct blob_attr *tb_sys_board[ARRAY_SIZE(board_policy)];
	blobmsg_parse(board_policy, ARRAY_SIZE(board_policy), tb_sys_board,
				blob_data(result_msg), blob_len(result_msg));

	struct blob_attr *tb_rel[ARRAY_SIZE(board_rel_policy)];
	blobmsg_parse(board_rel_policy, ARRAY_SIZE(board_rel_policy),
			tb_rel, blobmsg_data(tb_sys_board[BOARD_RELEASE]),
			blobmsg_data_len(tb_sys_board[BOARD_RELEASE]));
	char platver_buf[100];
	sprintf(platver_buf, "%s %s",
			blobmsg_get_string(tb_rel[BOARD_REL_VER]),
			blobmsg_get_string(tb_rel[BOARD_REL_REV]));

	blobmsg_buf_init(&output_buf);
	blobmsg_add_string(&output_buf, "name", blobmsg_get_string(tb_sys_board[BOARD_HOSTNAME]));
	if (use_model)
		blobmsg_add_string(&output_buf, "displayName", blobmsg_get_string(tb_sys_board[BOARD_MODEL]));

	/* "meta" object */
	tbl = blobmsg_open_table(&output_buf, "meta");
	blobmsg_add_string(&output_buf, "agent-name",
			strlen(agent_name) > 0 ? agent_name : "(unknown)");
	blobmsg_add_string(&output_buf, "agent-version",
			strlen(agent_ver) > 0 ? agent_ver : "(unknown)");
	/* CPU array */
	FILE *fp;
	if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
		fprintf(stderr, "err: failed to open \"/proc/cpuinfo\"\n");
		return -3;
	}
	ary = blobmsg_open_array(&output_buf, "cpu");
	char cpu_line[256];
	while (fgets(cpu_line, sizeof(cpu_line), fp) != NULL) {
		/*
		 * search "processor\t:" or "processor\t\t:" from /proc/cpuinfo
		 *   WZR-900DHP (BCM47081): 1x tab
		 *   ETG3-R (AR9342)/WN-DX1167R (MT7621A): 2x tabs
		 */
		if (strstr(cpu_line, "processor\t:") || strstr(cpu_line, "processor\t\t:")) {
			tbl2 = blobmsg_open_table(&output_buf, NULL);
			blobmsg_add_string(&output_buf, "model_name", blobmsg_get_string(tb_sys_board[BOARD_SYSTEM]));
			blobmsg_close_table(&output_buf, tbl2);
		}
	}
	fclose(fp);

	blobmsg_close_array(&output_buf, ary);
	/* end CPU */
	/* Kernel object */
	struct utsname uname_info;
	ret = uname(&uname_info);
	if (ret)
		return -2;
	tbl2 = blobmsg_open_table(&output_buf, "kernel");
	blobmsg_add_string(&output_buf, "machine", uname_info.machine);
	blobmsg_add_string(&output_buf, "name", uname_info.sysname);
	blobmsg_add_string(&output_buf, "os", "GNU/Linux");
	blobmsg_add_string(&output_buf, "platform_name", blobmsg_get_string(tb_rel[BOARD_REL_DIST]));
	blobmsg_add_string(&output_buf, "platform_version", platver_buf);
	blobmsg_add_string(&output_buf, "release", uname_info.release);
	blobmsg_add_string(&output_buf, "version", uname_info.version);
	blobmsg_close_table(&output_buf, tbl2);
	free(result_msg);
	/* end Kernel */
	/* Memory obect */
	ret = ubus_lookup_call("system", "info", NULL);
	if (ret)
		return ret;
	struct blob_attr *tb_sys_info[ARRAY_SIZE(sinfo_policy)];
	blobmsg_parse(sinfo_policy, ARRAY_SIZE(sinfo_policy), tb_sys_info,
				blob_data(result_msg), blob_len(result_msg));

	struct blob_attr *tb_mem[ARRAY_SIZE(sinfo_mem_policy)];
	blobmsg_parse(sinfo_mem_policy, ARRAY_SIZE(sinfo_mem_policy),
				tb_mem, blobmsg_data(tb_sys_info[SINFO_MEM]),
				blobmsg_data_len(tb_sys_info[SINFO_MEM]));

	char mem_total[100];
	sprintf(mem_total, "%llukB", blobmsg_get_u64(tb_mem[SINFO_MEM_TOTAL]) / 1024);
	tbl2 = blobmsg_open_table(&output_buf, "memory");
	blobmsg_add_string(&output_buf, "total", mem_total);
	blobmsg_close_table(&output_buf, tbl2);
	free(result_msg);
	/* end Memory */
	blobmsg_close_table(&output_buf, tbl);
	/* end "meta" */
	/* Interfaces array */
	ret = ubus_lookup_call("network.interface", "dump", NULL);
	if (ret)
		return ret;
	static struct blob_attr *ifdump_msg;	// declare new blob_attr* "ifdump_msg"
	ifdump_msg = blob_memdup(result_msg);	// and duplicate result_msg to ifdump_msg
	free(result_msg);					// and release result_msg for next step (l3device)
	struct blob_attr *tb_ifdump[ARRAY_SIZE(if_dump_policy)];
	blobmsg_parse(if_dump_policy, ARRAY_SIZE(if_dump_policy), tb_ifdump,
				blob_data(ifdump_msg), blob_len(ifdump_msg));

	ary = blobmsg_open_array(&output_buf, "interfaces");
	
	struct blob_attr *tb;
	char ifname[IFNAME_MAX_LEN], l3dev[DEVNAME_MAX_LEN], macaddr[17];
	unsigned rem;
	blobmsg_for_each_attr(tb, tb_ifdump[IFACE_DUMP], rem) {
		struct blob_attr *tb_tmp[ARRAY_SIZE(if_policy)];
		blobmsg_parse(if_policy, ARRAY_SIZE(if_policy), tb_tmp,
					blobmsg_data(tb), blobmsg_data_len(tb));
		strcpy(ifname, blobmsg_get_string(tb_tmp[IFACE_INTERFACE]));
		strcpy(l3dev, blobmsg_get_string(tb_tmp[IFACE_L3DEV]));
		if (!strcmp(ifname, "loopback")) {
			continue;
		}
//		fprintf(stderr, "ifname: %s, l3dev: %s\n", ifname, l3dev);
		struct blob_attr *tb_l3dev[ARRAY_SIZE(l3dev_policy)];
		ret = get_l3dev_status(l3dev);
		if (ret)
			return ret;
		blobmsg_parse(l3dev_policy, ARRAY_SIZE(l3dev_policy), tb_l3dev,
					blob_data(result_msg), blob_len(result_msg));
		strcpy(macaddr, blobmsg_get_string(tb_l3dev[L3DEV_MACADDR]));
		if (result_msg)
			free(result_msg);

		/* interface child object */
		tbl = blobmsg_open_table(&output_buf, NULL);

		char ifname_disp[128];
		sprintf(ifname_disp, "%s (%s)", ifname, l3dev);
		struct blob_attr *tb2;
		unsigned rem2;
		blobmsg_add_string(&output_buf, "name", ifname_disp);
		blobmsg_add_string(&output_buf, "macAddress", macaddr);
		/* v4 address array in interface */
		ary2 = blobmsg_open_array(&output_buf, "ipv4Addresses");
			// get from ipv4-address[]
			blobmsg_for_each_attr(tb2, tb_tmp[IFACE_V4ADDRS], rem2) {
				struct blob_attr *tb_if_addr[_IFACE_ADDR_MAX];
				blobmsg_parse(if_addr_policy, _IFACE_ADDR_MAX, tb_if_addr,
						blobmsg_data(tb2), blobmsg_data_len(tb2));

				blobmsg_add_string(&output_buf, NULL,
						blobmsg_get_string(tb_if_addr[IFACE_ADDR_ADDRESS]));
			}
		blobmsg_close_array(&output_buf, ary2);
		/* end v4 address */
		/* v6 address array in interface */
		ary2 = blobmsg_open_array(&output_buf, "ipv6Addresses");
			// get from ipv6-address[]
			blobmsg_for_each_attr(tb2, tb_tmp[IFACE_V6ADDRS], rem2) {
				struct blob_attr *tb_if_addr[_IFACE_ADDR_MAX];
				blobmsg_parse(if_addr_policy, _IFACE_ADDR_MAX, tb_if_addr,
						blobmsg_data(tb2), blobmsg_data_len(tb2));

				blobmsg_add_string(&output_buf, NULL,
						blobmsg_get_string(tb_if_addr[IFACE_ADDR_ADDRESS]));
			}
			// get from ipv6-prefix-assignment[]
			blobmsg_for_each_attr(tb2, tb_tmp[IFACE_V6ASSIGN], rem2) {
				struct blob_attr *tb_if_addr[_IFACE_ADDR_MAX];
				blobmsg_parse(if_addr_policy, _IFACE_ADDR_MAX, tb_if_addr,
						blobmsg_data(tb2), blobmsg_data_len(tb2));
				struct blob_attr *tb_if_addr_asn[_IFACE_ADDR_MAX];
				blobmsg_parse(if_addr_policy, _IFACE_ADDR_MAX, tb_if_addr_asn,
						blobmsg_data(tb_if_addr[IFACE_ADDR_LADDRESS]),
						blobmsg_data_len(tb_if_addr[IFACE_ADDR_LADDRESS]));
				blobmsg_add_string(&output_buf, NULL,
						blobmsg_get_string(tb_if_addr_asn[IFACE_ADDR_ADDRESS]));
			}
		blobmsg_close_array(&output_buf, ary2);
		/* end v6 address */

		blobmsg_close_table(&output_buf, tbl);
		/* end child object */

	}

	blobmsg_close_array(&output_buf, ary);
	/* end Interfaces */
	free(ifdump_msg);

	char *json = blobmsg_format_json_indent(output_buf.head, true, formatted ? 0 : -1);
	printf("%s\n", json);

	return 0;
}

static int get_sys_stat(void)
{
	int i, ret;
	char cpuinfo_path[] = "/proc/stat";
	FILE *fp;
	void *tbl, *tbl2;

	if ((fp = fopen(cpuinfo_path, "r")) == NULL) {
		fprintf(stderr, "err: failed to open \"%s\"\n", cpuinfo_path);
		return -3;
	}

	char line[256];
	if (fgets(line, sizeof(line), fp) == NULL) {
		fprintf(stderr, "err: failed to get cpu stat\n");
		return -3;
	}

	unsigned long long val;
	/* skip first block ("cpu") */
	strtok(line, " ");

	blobmsg_buf_init(&tmp_buf);

	/* "cpu" object */
	tbl = blobmsg_open_table(&tmp_buf, "cpu");

	for (i = 0; i < _SSTAT_CPU_MAX; i++) {
		val = strtoull(strtok(NULL, " "), NULL, 10);
		blobmsg_add_u64(&tmp_buf, sstat_cpu_policy[i].name, val);
	}
	blobmsg_close_table(&tmp_buf, tbl);
	/* end "cpu" */

	/* "if" array */
	tbl = blobmsg_open_table(&tmp_buf, "if");
	ret = ubus_lookup_call("network.interface", "dump", NULL);
	if (ret)
		return ret;
	struct blob_attr *ifdump_msg;
	ifdump_msg = blob_memdup(result_msg);
	free(result_msg);

	struct blob_attr *tb_ifdump[ARRAY_SIZE(if_dump_policy)];
	blobmsg_parse(if_dump_policy, ARRAY_SIZE(if_dump_policy),
			tb_ifdump, blob_data(ifdump_msg), blob_len(ifdump_msg));

	ret = blobmsg_check_array(tb_ifdump[IFACE_DUMP], BLOBMSG_TYPE_TABLE);
	if (ret < 1)
		return -1;
//	fprintf(stderr, "check ret: %d\n", ret);

	struct blob_attr *tb;
	char l3dev_exists[DEVNAME_MAX_LEN][ret];
	char l3dev[DEVNAME_MAX_LEN];
	unsigned rem;
	int index = 0;
	blobmsg_for_each_attr(tb, tb_ifdump[IFACE_DUMP], rem) {
		struct blob_attr *tb_tmp[ARRAY_SIZE(if_policy)];
		blobmsg_parse(if_policy, ARRAY_SIZE(if_policy), tb_tmp,
				blobmsg_data(tb), blobmsg_data_len(tb));
		strcpy(l3dev, blobmsg_get_string(tb_tmp[IFACE_L3DEV]));
		if (!strcmp(blobmsg_get_string(tb_tmp[IFACE_INTERFACE]), "loopback"))
			continue;

		bool existed = false;
		for (i = 0; i < index; i++) {
			if (!strcmp(l3dev_exists[i], l3dev)) {
				existed = true;
				break;
			}
		}
		if (existed)
			continue;
		strcpy(l3dev_exists[index], l3dev);

		/* "if" child object */
		tbl2 = blobmsg_open_table(&tmp_buf, l3dev);

		struct blob_attr *tb_l3dev[ARRAY_SIZE(l3dev_policy)];
		ret = get_l3dev_status(l3dev);
		if (ret)
			return ret;
		blobmsg_parse(l3dev_policy, ARRAY_SIZE(l3dev_policy), tb_l3dev,
				blob_data(result_msg), blob_len(result_msg));

		struct blob_attr *tb_l3dev_stat[ARRAY_SIZE(l3dev_stat_policy)];
		blobmsg_parse(l3dev_stat_policy, ARRAY_SIZE(l3dev_stat_policy),
				tb_l3dev_stat, blobmsg_data(tb_l3dev[L3DEV_STAT]),
				blobmsg_data_len(tb_l3dev[L3DEV_STAT]));
		unsigned long long txb, rxb;
		char txb_buf[20], rxb_buf[20];
		txb = blobmsg_get_u64(tb_l3dev_stat[L3DEV_STAT_TXB]);
		rxb = blobmsg_get_u64(tb_l3dev_stat[L3DEV_STAT_RXB]);
		sprintf(txb_buf, "%llu", txb);
		sprintf(rxb_buf, "%llu", rxb);
		blobmsg_add_string(&tmp_buf, "tx_bytes", txb_buf);
		blobmsg_add_string(&tmp_buf, "rx_bytes", rxb_buf);

		blobmsg_close_table(&tmp_buf, tbl2);
		/* end "if" child */

		index++;
	}
	free(ifdump_msg);
	if (result_msg)
		free(result_msg);
	blobmsg_close_table(&tmp_buf, tbl);
	/* end "if" */

//	char *json = blobmsg_format_json_indent(tmp_buf.head, true, formatted ? 0 : -1);
//	printf("%s\n", json);

	return 0;
}

static int load_sysstat_json(void)
{
	char *buf;
	FILE *fp;
	struct stat st;

	if ((fp = fopen(jsonpath, "r")) == NULL) {
		fprintf(stderr, "warn: failed to open \"%s\"\n", jsonpath);
		return -3;
	}
	if (stat(jsonpath, &st)) {
		fprintf(stderr, "err: failed to stat json file\n");
		fclose(fp);
		return -3;
	}
	if (!(buf = malloc(st.st_size))) {
		fprintf(stderr, "err: failed to allocate memory for loading json\n");
		fclose(fp);
		return -3;
	}
	if (fread(buf, 1, st.st_size, fp) != st.st_size) {
		fprintf(stderr, "err: failed to read from json file\n");
		free(buf);
		fclose(fp);
		return -3;
	}
	fclose(fp);

	blobmsg_buf_init(&load_buf);
	if (!blobmsg_add_json_from_string(&load_buf, buf)) {
		fprintf(stderr, "err: failed to parse loaded json\n");
		return -1;
	}
//	char *json = blobmsg_format_json_indent(load_buf.head, true, formatted ? 0 : -1);
//	fprintf(stderr, "load_buf: %s\n", json);

	return 0;
}

static void
add_metric_object(char *name, uint64_t time, void *value, int type)
{
	void *tbl;
	/* for u64, based on blob_get_u64() */
	uint32_t *ptr = (uint32_t *) value;
	uint64_t tmp = ptr[0];
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
	tmp |= ((uint64_t) ptr[1]) << 32;
#else
	tmp = tmp << 32;
	tmp |= ptr[1];
#endif
	/* for double, based on blobmsg_get_double() */
	union {
		double d;
		uint64_t u64;
	} v;
	v.u64 = tmp;

	if (type != BLOBMSG_TYPE_INT32 && type != BLOBMSG_TYPE_INT64 &&
		type != BLOBMSG_TYPE_DOUBLE)
		return;
	tbl = blobmsg_open_table(&output_buf, NULL);
	blobmsg_add_string(&output_buf, "hostId", hostid);
	blobmsg_add_string(&output_buf, "name", name);
	blobmsg_add_u64(&output_buf, "time", time);
	switch (type) {
		case BLOBMSG_TYPE_INT32:
			blobmsg_add_u32(&output_buf, "value", (uint32_t)value);
			break;
		case BLOBMSG_TYPE_INT64:
			blobmsg_add_u64(&output_buf, "value", tmp);
//			fprintf(stderr, "add uint64_t: %llu (%llx)\nptr[0]: %x (%u), ptr[1]: %x (%u)\n",
//					tmp, tmp, *ptr, *ptr, *(ptr + 1), *(ptr + 1));
			break;
		case BLOBMSG_TYPE_DOUBLE:
			blobmsg_add_double(&output_buf, "value", v.d);
//			fprintf(stderr, "add double: %lf\n", v.d);
			break;
	}
	blobmsg_close_table(&output_buf, tbl);
}

static void print_metric_json(void)
{
	if (!output_buf.head)
		return;

	struct blob_attr *tb_metric[_METRIC_MAX];
	blobmsg_parse(metric_policy, _METRIC_MAX, tb_metric,
			blobmsg_data(output_buf.head),
			blobmsg_data_len(output_buf.head));

	if (!tb_metric[METRIC_METRICS]) {
		fprintf(stderr, "err: no metrics (null)\n");
		return;
	}

//	fprintf(stderr, "---- output_buf ----\n");
	printf("%s\n", blobmsg_format_json_indent(tb_metric[METRIC_METRICS], true, formatted ? 0 : -1));
//	fprintf(stderr, "--------------------\n");
}

static int get_metric_stat(bool loaded)
{
	int i = 0, ret;
	unsigned rem, rem2;
	char metric[32];
	struct blob_attr *tb, *tb2;
	void *ary;

	blobmsg_buf_init(&output_buf);

	/* open "metrics" array */
	ary = blobmsg_open_array(&output_buf, "metrics");
	/* start loadavg and Memory */
	ret = ubus_lookup_call("system", "info", NULL);
	if (ret)
		return ret;
	
	struct blob_attr *tb_sys_info[ARRAY_SIZE(sinfo_policy)];
	blobmsg_parse(sinfo_policy, ARRAY_SIZE(sinfo_policy), tb_sys_info,
			blob_data(result_msg), blob_len(result_msg));
	
	ret = blobmsg_check_array(tb_sys_info[SINFO_LOAD], 0);
	if (ret < 1) {
		fprintf(stderr, "err: failed to check: %d\n", ret);
		return -1;
	}

	/* loadavg */
	double load;
	int load_time[] = { 1, 5, 15 };
	blobmsg_for_each_attr(tb, tb_sys_info[SINFO_LOAD], rem) {
		load = blobmsg_get_u32(tb) / 65535.0;

		sprintf(metric, "loadavg%d", load_time[i]);
		add_metric_object(metric, time(NULL), &load, BLOBMSG_TYPE_DOUBLE);
		i++;
	}
	/* end loadavg */

	/* memory */
	unsigned long long total, avail, used;
	struct blob_attr *tb_mem[ARRAY_SIZE(sinfo_mem_policy)];
	blobmsg_parse(sinfo_mem_policy, ARRAY_SIZE(sinfo_mem_policy),
			tb_mem, blobmsg_data(tb_sys_info[SINFO_MEM]),
			blobmsg_data_len(tb_sys_info[SINFO_MEM]));
	total = blobmsg_get_u64(tb_mem[SINFO_MEM_TOTAL]);
	avail = blobmsg_get_u64(tb_mem[SINFO_MEM_AVAILABLE]);
	used = total - avail;

//	fprintf(stderr, "total: %llu, avail: %llu, used: %llu\n", total, avail, used);
	add_metric_object("memory.total", time(NULL), &total,
			BLOBMSG_TYPE_INT64);
	add_metric_object("memory.mem_available", time(NULL), &avail,
			BLOBMSG_TYPE_INT64);
	add_metric_object("memory.used", time(NULL), &used,
			BLOBMSG_TYPE_INT64);
	/* end memory */
	free(result_msg);

	/* check if the json is loaded from the file */
	if (!loaded) {
//		fprintf(stderr, "no json loaded\n");
		blobmsg_close_array(&output_buf, ary);
		/* close "metric" */
		return 0;
	}
//	fprintf(stderr, "json loaded\n");

	/* start CPU and Interfaces */
	struct blob_attr *tb_load_sstat[_SSTAT_MAX];
	blobmsg_parse(sstat_policy, _SSTAT_MAX, tb_load_sstat,
			blob_data(load_buf.head), blob_len(load_buf.head));

	struct blob_attr *tb_cur_sstat[_SSTAT_MAX];
	blobmsg_parse(sstat_policy, _SSTAT_MAX, tb_cur_sstat,
			blob_data(tmp_buf.head), blob_len(tmp_buf.head));
	struct blob_attr *tb_cur_cpu[_SSTAT_CPU_MAX];
	blobmsg_parse_array(sstat_cpu_policy, _SSTAT_CPU_MAX,
			tb_cur_cpu, blobmsg_data(tb_cur_sstat[SSTAT_CPU]),
			blobmsg_data_len(tb_cur_sstat[SSTAT_CPU]));

	int type;
	uint64_t value_l, value_c, diff_total = 0, value_diffs[_SSTAT_CPU_MAX];
	i = 0;
	blobmsg_for_each_attr(tb, tb_load_sstat[SSTAT_CPU], rem) {
		type = blobmsg_type(tb);
		value_l = type == BLOBMSG_TYPE_INT32 ? 
				(uint64_t)blobmsg_get_u32(tb) : blobmsg_get_u64(tb);
		value_c = blobmsg_get_u64(tb_cur_cpu[i]);

		diff_total += value_diffs[i] = value_c - value_l;
		i++;
	}
//	printf("diff_total: %llu\n", diff_total);
	double p;
	for (i = 0; i < _SSTAT_CPU_MAX; i++) {
		p = value_diffs[i] * 100.00 / diff_total;
		sprintf(metric, "cpu.%s.percentage", sstat_cpu_policy[i].name);
		add_metric_object(metric, time(NULL), &p, BLOBMSG_TYPE_DOUBLE);
	}

	char l3dev_l[DEVNAME_MAX_LEN], l3dev_c[DEVNAME_MAX_LEN];
	uint64_t xxb_l[_SSTAT_IF_MAX], xxb_c[_SSTAT_IF_MAX], xxb_diff;
	struct blob_attr *tb_tmp;
	blobmsg_for_each_attr(tb, tb_load_sstat[SSTAT_IF], rem) {
		strcpy(l3dev_l, blobmsg_name(tb));

		blobmsg_for_each_attr(tb2, tb_cur_sstat[SSTAT_IF], rem2) {
			strcpy(l3dev_c, blobmsg_name(tb2));
			if (strcmp(l3dev_l, l3dev_c))
				continue;

			struct blob_attr *tb_cur_if[_SSTAT_IF_MAX];
			blobmsg_parse(sstat_if_policy, _SSTAT_IF_MAX, tb_cur_if,
					blobmsg_data(tb2), blobmsg_data_len(tb2));
			xxb_c[0] = cnv_xxb(blobmsg_get_string(tb_cur_if[SSTAT_IF_TXB]));
			xxb_c[1] = cnv_xxb(blobmsg_get_string(tb_cur_if[SSTAT_IF_RXB]));

			break;
		}

		struct blob_attr *tb_load_if[_SSTAT_IF_MAX];
		blobmsg_parse(sstat_if_policy, _SSTAT_IF_MAX, tb_load_if,
				blobmsg_data(tb), blobmsg_data_len(tb));
		i = 0;
		blobmsg_for_each_attr(tb_tmp, tb, rem2) {
			type = blobmsg_type(tb_tmp);
			xxb_l[i] = cnv_xxb(blobmsg_get_string(tb_tmp));
			i++;
		}
		xxb_diff = xxb_c[0] - xxb_l[0];
		sprintf(metric, "interface.%s.txBytes.delta", cnv_devname(l3dev_l));
		add_metric_object(metric, time(NULL), &xxb_diff, BLOBMSG_TYPE_INT64);
		xxb_diff = xxb_c[1] - xxb_l[1];
		sprintf(metric, "interface.%s.rxBytes.delta", cnv_devname(l3dev_l));
		add_metric_object(metric, time(NULL), &xxb_diff, BLOBMSG_TYPE_INT64);
	}
	blobmsg_close_array(&output_buf, ary);
	/* close "metrics" */

	return 0;
}

int main(int argc, char **argv)
{
	int opt, ret = 0;
	char *cmd;

	/* defaults */
#ifdef AGENT_NAME
	strcpy(agent_name, AGENT_NAME);
#endif
#ifdef AGENT_VER
	strcpy(agent_ver, AGENT_VER);
#endif
	char jsonpath_def[] = "/tmp/ma-sysstat.json";
	jsonpath = jsonpath_def;
	uint32_t timeout_buf;

	while ((opt = getopt(argc, argv, "Fh:j:mt:")) != -1) {
		switch(opt) {
			case 'F':
				formatted = true;
				break;
			case 'h':
				if (!optarg || strlen(optarg) != 11) {
					fprintf(stderr, "err: invalid Host ID\n");
					return -1;
				}
				strcpy(hostid, optarg);
				break;
			case 'j':
				jsonpath = optarg;
				break;
			case 'm':
				use_model = true;
				break;
			case 't':
				timeout_buf = strtoul(optarg, NULL, 10);
				if (timeout_buf <= 0 || timeout_buf == ULONG_MAX) {
					fprintf(stderr,
						"warning: invalid timeout value (must be > 0), use default (%us)\n", timeout);
					break;
				}
				timeout = timeout_buf;
				break;
			default:
				fprintf(stderr, "err: unknown paramerter\n");
				return -1;
		}
	}

	argc -= optind;
	argv += optind;

	cmd = argv[0];
	if (argc < 1)
		return -1;

//	fprintf(stderr, "agent_name: %s\n", agent_name);
//	fprintf(stderr, "agent_ver: %s\n", agent_ver);
//	fprintf(stderr, "cmd: %s\n", cmd);
//	fprintf(stderr, "formatted: %s\n", formatted ? "true" : "false");
//	fprintf(stderr, "use_model: %s\n", use_model ? "true" : "false");

	ctx = ubus_connect(NULL);
	if(!ctx) {
		fprintf (stderr, "err: failed to connect to ubus\n");
		return -1;
	}

	if (!strcmp(cmd, "systemj")) {
		ret = print_sysinfo_json();
		if (ret) {
			fprintf(stderr, "err: failed to get system json (%s)\n",
					ubus_strerror(ret));
		}
	} else if (!strcmp(cmd, "metricj"))
	{
		if (strlen(hostid) != 11) {
			fprintf(stderr, "err: no Host ID is specified\n");
			free(ctx);
			return -1;
		}
		ret = get_sys_stat();
		if (ret) {
			fprintf(stderr, "err: failed to get system status (%s)\n",
					ubus_strerror(ret));
			free(ctx);
			return ret;
		}
		ret = load_sysstat_json();
		ret = get_metric_stat(ret ? false : true);
		if (ret) {
			fprintf(stderr, "err: failed to get metric data (%s)\n",
					ubus_strerror(ret));
			free(ctx);
			return ret;
		}
		print_metric_json();
		FILE *fp;
		if ((fp = fopen(jsonpath, "w")) == NULL) {
			fprintf(stderr, "err: failed to open the temporary json file for writing\n");
			free(ctx);
			return -3;
		}
		char *json = blobmsg_format_json_indent(tmp_buf.head, true, formatted ? 0 : -1);
		if (!fwrite(json, strlen(json), 1, fp)) {
			fprintf(stderr, "err: failed to write to temporary json file\n");
			ret = -3;
		}
		fclose(fp);
	} else if (!strcmp(cmd, "debug"))
	{
		/* debug code */
	}
	
	free(ctx);

	return ret;
}
