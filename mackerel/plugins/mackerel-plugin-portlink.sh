#!/bin/sh

# Get switches
SWITCHES=$(uci -q show network | \
	grep -E "network\.@switch\[\d\]\.name" | \
	sed "s/network\.@switch\[\d\]\.name='\(switch[0-9]\+\)'/\1/")

# graph define
PLUGIN_NAME="portlink"
cnt="0"
for sw in $SWITCHES; do
	if [ ! "$cnt" = "0" ]; then
		SW_GRAPH_DEF="${SW_GRAPH_DEF},"
	fi
	SW_GRAPH_DEF="${SW_GRAPH_DEF}\"${PLUGIN_NAME}.${sw}.#\":{\"label\":\"Ethernet LinkSpeed on ${sw} (Mbps)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":false}]}"
	cnt=$(( cnt + 1 ))
done

GRAPH_DEF="# mackerel-agent-plugin\n"
GRAPH_DEF="${GRAPH_DEF}{\"graphs\":{"
GRAPH_DEF="${GRAPH_DEF}${SW_GRAPH_DEF}"
GRAPH_DEF="${GRAPH_DEF}}}"

if [ -n "$MACKEREL_AGENT_PLUGIN_META" ]; then
	echo -e "${GRAPH_DEF}"
	exit 0
fi

SECONDS=$(date '+%s')

# Get VLAN configurations
VLAN_CFGS=$(uci -q show network | \
	grep -E "network\.@switch_vlan\[\d\]=switch_vlan" | \
	sed "s/network\.@switch_vlan\[\(\d\)\]=switch_vlan/\1/")

for sw in $SWITCHES; do
	SW_NUM="$(echo $sw | tr -d "[a-z]")"	# e.g.: switch0 -> 0

	for cfg in $VLAN_CFGS; do
		# Get switch device from vlans
		SW_DEV=$(uci -q get network.@switch_vlan[${cfg}].device)

		if [ "$SW_DEV" = "${sw}" ]; then
			# VLAN ID (e.g.: eth0."1")
			SW_VLAN_ID="$(uci -q get network.@switch_vlan[${cfg}].vlan)"
			# ports in vlan
			SW_VLAN_PORTS=$(uci -q get network.@switch_vlan[${cfg}].ports | sed "s/\d\+t//g")
			ETH="${SW_NUM}_${SW_VLAN_ID}"	# e.g.: 0.1 -> 0_1

			for port in $SW_VLAN_PORTS; do
				PREFIX="${PLUGIN_NAME}.${sw}.eth${ETH}.port${port}"
				LINK_STATUS=`swconfig dev $sw port $port get link | grep -oE "link:(\w+)" | grep -oE "(down|up)"`
				LINK_SPEED=`swconfig dev $sw port $port get link | grep -oE "speed:(\d+)" | grep -oE "\d+"`

				# Set LINK_SPEED=0 if port status is "down"
				if [ "$LINK_STATUS" = "down" ]; then
					LINK_SPEED=0
				fi
				echo -e "${PREFIX}\t${LINK_SPEED}\t${SECONDS}"
			done
		fi
	done
done
