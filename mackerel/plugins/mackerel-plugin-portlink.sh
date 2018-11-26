#!/bin/sh

. /usr/share/libubox/jshn.sh

PLUGIN_NAME="portlink"

get_sw_portspeed() {
	local sw_index="$1"
	local port="$2"

	local port_link="$(swconfig dev $sw_index port $port get link 2> /dev/null)"
	local port_status="$(echo $port_link | grep -oE "link:(\w+)" | sed "s/link:\(down\|up\)/\1/")"

	[ "$port_status" = "up" ] && \
		echo $port_link | grep -oE "speed:(\d+)" | sed "s/speed:\(\d\+\)/\1/" ||
		echo "0"
}

[ -r "/etc/board.json" ] || exit 1
json_init
json_load_file "/etc/board.json"

json_select switch
	json_get_keys switches
json_select ..

[ -z "$switches" ] && exit 0

# print graph definition
if [ -n "$MACKEREL_AGENT_PLUGIN_META" ]; then
	cnt="0"
	for sw in $switches; do
		if [ ! "$cnt" = "0" ]; then
			SW_GRAPH_DEF="${SW_GRAPH_DEF},"
		fi
		SW_GRAPH_DEF="${SW_GRAPH_DEF}\"${PLUGIN_NAME}.${sw}.#\":{\"label\":\"LinkSpeed on ${sw} (Mbps)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":false}]}"
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

for sw in $switches; do
	json_select switch
		json_select $sw
			json_get_keys port_array ports
			json_select ports
			for index in $port_array; do
				json_select "$index"
					json_get_var port num
					json_get_var eth device
					[ -z "$eth" ] && ports="$ports $port"
				json_select ..
			done
		json_select ..
	json_select ..

	for p in $ports; do
		link_speed="$(get_sw_portspeed $sw $p)"
		echo -e "${PLUGIN_NAME}.$sw.port$p\t$link_speed\t$SECONDS"
	done
done
