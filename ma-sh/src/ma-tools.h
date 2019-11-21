#ifndef TEST_H
#define TEST_H

#include <libubus.h>

#define MACADDR_LEN		17	/* xx:xx:xx:xx:xx:xx */
#define IFNAME_MAX_LEN	32	/* wan, lan, ... */
#define DEVNAME_MAX_LEN	32	/* eth0, eth0.2, br-lan, ... */

/* system board */
enum {
	BOARD_HOSTNAME,
	BOARD_KERNEL,
	BOARD_SYSTEM,
	BOARD_MODEL,
	BOARD_RELEASE,
};

static const struct blobmsg_policy board_policy[] = {
	[BOARD_HOSTNAME] = { .name = "hostname", .type = BLOBMSG_TYPE_STRING },
	[BOARD_KERNEL] = { .name = "kernel", .type = BLOBMSG_TYPE_STRING },
	[BOARD_SYSTEM] = { .name = "system" , .type = BLOBMSG_TYPE_STRING },
	[BOARD_MODEL] = { .name = "model", .type = BLOBMSG_TYPE_STRING },
	[BOARD_RELEASE] = { .name = "release", .type = BLOBMSG_TYPE_TABLE },
};

/* system board -> release */
enum {
	BOARD_REL_DIST,
	BOARD_REL_DESC,
	BOARD_REL_VER,
	BOARD_REL_REV,
};

static const struct blobmsg_policy board_rel_policy[] = {
	[BOARD_REL_DIST] = { .name = "distribution", .type = BLOBMSG_TYPE_STRING },
	[BOARD_REL_VER] = { .name = "version", .type = BLOBMSG_TYPE_STRING },
	[BOARD_REL_REV] = { .name = "revision", .type = BLOBMSG_TYPE_STRING },
	[BOARD_REL_DESC] = { .name = "description", .type = BLOBMSG_TYPE_STRING },
};

/* system info */
enum {
	SINFO_LTIME,
	SINFO_LOAD,
	SINFO_MEM,
	SINFO_SWAP,
};

static const struct blobmsg_policy sinfo_policy[] = {
	[SINFO_LTIME] = { .name = "localtime", .type = BLOBMSG_TYPE_INT64 },
	[SINFO_LOAD] = { .name = "load", .type = BLOBMSG_TYPE_ARRAY },
	[SINFO_MEM] = { .name = "memory", .type = BLOBMSG_TYPE_TABLE },
	[SINFO_SWAP] = { .name = "swap", .type = BLOBMSG_TYPE_TABLE },
};

/* system info -> memory */
enum {
	SINFO_MEM_TOTAL,
	SINFO_MEM_AVAILABLE,
};

static const struct blobmsg_policy sinfo_mem_policy[] = {
	[SINFO_MEM_TOTAL] = { .name = "total", .type = BLOBMSG_TYPE_INT64 },
	[SINFO_MEM_AVAILABLE] = { .name = "available", .type = BLOBMSG_TYPE_INT64 },
};

/* network.interface dump */
enum {
	IFACE_DUMP,
};

static const struct blobmsg_policy if_dump_policy[] = {
	[IFACE_DUMP] = { .name = "interface", .type = BLOBMSG_TYPE_ARRAY },
};

/* network.interface dump -> 0, 1, 2, ...
 * network.interface.* status (no IFACE_INTERFACE)
 */
enum {
	IFACE_INTERFACE,
	IFACE_L3DEV,
	IFACE_V4ADDRS,
	IFACE_V6ADDRS,
	IFACE_V6ASSIGN,
};

static const struct blobmsg_policy if_policy[] = {
	[IFACE_INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
	[IFACE_L3DEV] = { .name = "l3_device", .type = BLOBMSG_TYPE_STRING },
	[IFACE_V4ADDRS] = { .name = "ipv4-address", .type = BLOBMSG_TYPE_ARRAY },
	[IFACE_V6ADDRS] = { .name = "ipv6-address", .type = BLOBMSG_TYPE_ARRAY },
	[IFACE_V6ASSIGN] = { .name = "ipv6-prefix-assignment", .type = BLOBMSG_TYPE_ARRAY },
};

/* 
 * network.interface.* status -> ipv*-address
 * network.interface.* status -> ipv6-prefix-assignment -> local-address
 */
enum {
	IFACE_ADDR_ADDRESS,
	IFACE_ADDR_LADDRESS,
	_IFACE_ADDR_MAX,
};

static const struct blobmsg_policy if_addr_policy[] = {
	[IFACE_ADDR_ADDRESS] = { .name = "address", .type = BLOBMSG_TYPE_STRING },
	[IFACE_ADDR_LADDRESS] = { .name = "local-address", .type = BLOBMSG_TYPE_TABLE },
};

/* network.device status */
enum {
	L3DEV_MACADDR,
	L3DEV_STAT,
};

static const struct blobmsg_policy l3dev_policy[] = {
	[L3DEV_MACADDR] = { .name = "macaddr", .type = BLOBMSG_TYPE_STRING },
	[L3DEV_STAT] = { .name = "statistics", .type = BLOBMSG_TYPE_TABLE },
};

/* network.device status -> eth*, eth*, ... -> statistics */
enum {
	L3DEV_STAT_TXB,
	L3DEV_STAT_RXB,
};

static const struct blobmsg_policy l3dev_stat_policy[] = {
	[L3DEV_STAT_TXB] = { .name = "tx_bytes", .type = BLOBMSG_TYPE_INT64 },
	[L3DEV_STAT_RXB] = { .name = "rx_bytes", .type = BLOBMSG_TYPE_INT64 },
};

/* for parsing system status json */
enum {
	SSTAT_CPU,
	SSTAT_IF,
	_SSTAT_MAX,
};

static const struct blobmsg_policy sstat_policy[] = {
	[SSTAT_CPU] = { .name = "cpu", .type = BLOBMSG_TYPE_TABLE },
	[SSTAT_IF] = { .name = "if", .type = BLOBMSG_TYPE_TABLE },
};

enum {
	SSTAT_CPU_USR,
	SSTAT_CPU_NIC,
	SSTAT_CPU_SYS,
	SSTAT_CPU_IDLE,
	SSTAT_CPU_IO,
	SSTAT_CPU_IRQ,
	SSTAT_CPU_SIRQ,
	SSTAT_CPU_ST,
	_SSTAT_CPU_MAX,
};

static const struct blobmsg_policy sstat_cpu_policy[] = {
	[SSTAT_CPU_USR] = { .name = "user", .type = BLOBMSG_TYPE_INT64 },
	[SSTAT_CPU_NIC] = { .name = "nice", .type = BLOBMSG_TYPE_INT64 },
	[SSTAT_CPU_SYS] = { .name = "system", .type = BLOBMSG_TYPE_INT64 },
	[SSTAT_CPU_IDLE] = { .name = "idle", .type = BLOBMSG_TYPE_INT64 },
	[SSTAT_CPU_IO] = { .name = "iowait", .type = BLOBMSG_TYPE_INT64 },
	[SSTAT_CPU_IRQ] = { .name = "irq", .type = BLOBMSG_TYPE_INT64 },
	[SSTAT_CPU_SIRQ] = { .name = "softirq", .type = BLOBMSG_TYPE_INT64 },
	[SSTAT_CPU_ST] = { .name = "steal", .type = BLOBMSG_TYPE_INT64 },
};
/*
enum {
	SSTAT_IF_NAME,
	SSTAT_IF_STAT,
	_SSTAT_IF_MAX,
};*/

/*static const struct blobmsg_policy sstat_if_policy[] = {
	[SSTAT_IF_NAME] = { .name = "name", .type = BLOBMSG_TYPE_STRING },
	[SSTAT_IF_STAT] = { .name = "stat", .type = BLOBMSG_TYPE_TABLE },
};*/

enum {
	SSTAT_IF_TXB,
	SSTAT_IF_RXB,
	_SSTAT_IF_MAX,
};

static const struct blobmsg_policy sstat_if_policy[] = {
	[SSTAT_IF_TXB] = { .name = "tx_bytes", .type = BLOBMSG_TYPE_STRING },
	[SSTAT_IF_RXB] = { .name = "rx_bytes", .type = BLOBMSG_TYPE_STRING },
};

/* for parsing metric array data */
enum {
	METRIC_METRICS,
	_METRIC_MAX,
};

static const struct blobmsg_policy metric_policy[] = {
	[METRIC_METRICS] = { .name = "metrics", .type = BLOBMSG_TYPE_ARRAY },
};

static void
ubus_receive_result_cb(struct ubus_request *req, int type, struct blob_attr *msg);

static int
ubus_lookup_call(char *path_str, const char *method, struct blob_attr *attr);

static int get_l3dev_status(char *devname);

static int print_sysinfo_json(void);

#endif
