#!/bin/sh
SECONDS=`date '+%s'`

PLUGIN_NAME='appproto'
GRAPH_PREFIX="# mackerel-agent-plugin\n"
GRAPH_DEF="{\"graphs\":{"
GRAPH_DEF="${GRAPH_DEF}\"appproto.bytes.#\":{\"label\":\"Application Protocols (Bytes)\",\"unit\":\"bytes\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":true}]},"
GRAPH_DEF="${GRAPH_DEF}\"appproto.pkts.#\":{\"label\":\"Application Protocols (Packets)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":true}]}"
GRAPH_DEF="${GRAPH_DEF}}}"

if [ -n "$MACKEREL_AGENT_PLUGIN_META" ]; then
	echo -e ${GRAPH_PREFIX}${GRAPH_DEF}
	exit 0
fi
# Load json tool
. /usr/share/libubox/jshn.sh

PROTO="ICMP IGMP FTP SSH Telnet SMTP DNS HTTP POP2 POP3 NTP NetBIOS IMAP IMAP3"
PROTO="${PROTO} HTTPS SMBoverTCP FTPS IMAPS POP3S BitTorrent GRE ESP IPIP L2TPv3 Other"
for app in $PROTO; do
	eval rxBytes_"${app}"='0'
	eval rxPkts_"${app}"='0'
	eval txBytes_"${app}"='0'
	eval txPkts_"${app}"='0'
done

# Load json from nlbw
NLBW_JSON=$(nlbw -c json -g layer7)
json_load $NLBW_JSON

# Select "data" field
json_select data

# Get rows in data
json_get_keys rows

for row in $rows; do
	# Select row in data
	json_select $row
	# Get items in row (not using)
	json_get_keys items
	# Get values in row
	json_get_var nlbw_proto 8	# application protocol
	json_get_var rx_bytes 4		# rx bytes
	json_get_var rx_pkts 5		# rx packets
	json_get_var tx_bytes 6		# tx bytes
	json_get_var tx_pkts 7		# tx packets
	# Return to upper level
	json_select ..

	# Cut dash letter in $nlbw_proto if it's value is "SMB-over-TCP"
	if [ "$nlbw_proto" = "SMB-over-TCP" ]; then
		nlbw_proto="SMBoverTCP"
	fi
	# Set "Other" to $nlbw_proto if it's value is null
	if [ "$nlbw_proto" = "" ]; then
		nlbw_proto="Other"
	fi

	eval rxBytes_${nlbw_proto}=$rx_bytes
	eval rxPkts_${nlbw_proto}=$rx_pkts
	eval txBytes_${nlbw_proto}=$tx_bytes
	eval txPkts_${nlbw_proto}=$tx_pkts
done

for app in $PROTO; do
	rxBytes=$(eval echo \"\$rxBytes_$app\")
	rxPkts=$(eval echo \"\$rxPkts_$app\")
	txBytes=$(eval echo \"\$txBytes_$app\")
	txPkts=$(eval echo \"\$txPkts_$app\")
	if [ "$rxBytes" != "0" ] || [ "$rxPkts" != "0" ] || [ "$txBytes" != "0" ] || [ "$txPkts" != "0" ] ; then
		echo -e "${PLUGIN_NAME}.bytes.${app}.rxBytes\t${rxBytes}\t${SECONDS}"
		echo -e "${PLUGIN_NAME}.pkts.${app}.rxPkts\t${rxPkts}\t${SECONDS}"
		echo -e "${PLUGIN_NAME}.bytes.${app}.txBytes\t${txBytes}\t${SECONDS}"
		echo -e "${PLUGIN_NAME}.pkts.${app}.txPkts\t${txPkts}\t${SECONDS}"
	fi
done
