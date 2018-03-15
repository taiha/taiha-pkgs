#!/bin/sh

# Get switches
# 0 1 2 ...
SWITCHES=$(uci -q show network | \
	grep -E "network\.@switch\[\d\]=switch" | \
	sed "s/network\.@switch\[\(\d\)\]=switch/\1/")

PLUGIN_NAME="portlink"

get_sw_property(){
	local sw_index="$1"
	local sw_property="$2"

	uci -q get network.@switch["${sw_index}"].$sw_property 2> /dev/null
}

get_vlan_property(){
	local vlan_index="$1"
	local vlan_property="$2"

	uci -q get network.@switch_vlan["${vlan_index}"].$vlan_property 2> /dev/null
}

get_sw_portspeed(){
	local sw_index="$1"
	local port="$2"

	local port_link="$(swconfig dev $sw_index port $port get link 2> /dev/null)"
	local port_status="$(echo $port_link | grep -oE "link:(\w+)" | sed "s/link:\(down\|up\)/\1/")"

	if [ "$port_status" = "down" ]; then	# port is down
		echo "0"
	else								# port is up
		echo $port_link | grep -oE "speed:(\d+)" | sed "s/speed:\(\d\+\)/\1/"
	fi
}

if [ -n "$MACKEREL_AGENT_PLUGIN_META" ]; then
	cnt="0"
	for sw in $SWITCHES; do
		sw_name="$(get_sw_property $sw "name")"
		if [ ! "$cnt" = "0" ]; then
			SW_GRAPH_DEF="${SW_GRAPH_DEF},"
		fi
		SW_GRAPH_DEF="${SW_GRAPH_DEF}\"${PLUGIN_NAME}.${sw_name}.#\":{\"label\":\"Ethernet LinkSpeed on ${sw_name} (Mbps)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":false}]}"
		cnt=$(( cnt + 1 ))
	done

	GRAPH_DEF="# mackerel-agent-plugin\n"
	GRAPH_DEF="${GRAPH_DEF}{\"graphs\":{"
	GRAPH_DEF="${GRAPH_DEF}${SW_GRAPH_DEF}"
	GRAPH_DEF="${GRAPH_DEF}}}"

	echo -e "${GRAPH_DEF}"
	exit 0
fi

SECONDS=$(date '+%s')

# Get VLAN configurations
VLAN_CFGS=$(uci -q show network | \
	grep -E "network\.@switch_vlan\[\d\]=switch_vlan" | \
	sed "s/network\.@switch_vlan\[\(\d\)\]=switch_vlan/\1/")

for sw in $SWITCHES; do
	sw_name="$(get_sw_property $sw "name")"	# "0" -> "switch0"

	for vlan in $VLAN_CFGS; do
		# Get switch device from vlans
		sw_dev=$(get_vlan_property $vlan "device")

		if [ "$sw_dev" = "$sw_name" ]; then
			# VLAN ID (e.g.: eth0."1")
			vlan_id="$(get_vlan_property "${vlan}" "vlan")"
			# ports in vlan
			vlan_ports="$(get_vlan_property "${vlan}" "ports" | sed "s/\d\+t//g")"
			vlan_eth="${sw}_${vlan_id}"	# e.g.: 0.1 -> 0_1

			for port in $vlan_ports; do
				prefix="${PLUGIN_NAME}.${sw_name}.eth${vlan_eth}.port${port}"
				link_speed="$(get_sw_portspeed $sw_name $port)"

				echo -e "${prefix}\t${link_speed}\t${SECONDS}"
			done
		fi
	done
done
