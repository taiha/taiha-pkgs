#!/bin/sh
# ref: https://github.com/ymyzk/mackerel-plugins/blob/master/raspberrypi/temp.sh
SECONDS=`date '+%s'`

SWNAME='portlink.switch0.'
PORTS=`for i in $(seq 0 5) ;do uci -q get network.@switch_vlan[$i].ports ; done | grep -oE "[0-9][^t]"`

if [ -n "$MACKEREL_AGENT_PLUGIN_META" ]; then
     echo -e "# mackerel-agent-plugin\n{\"graphs\":{\"portlink.switch0\":{\"label\":\"Ethernet LinkSpeed (switch0)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":false}]}}}"
else
     for p in $PORTS; do
          NAME=${SWNAME}port$p
          LINK_STATUS=`swconfig dev switch0 port $p get link | grep -oE "link:(\w+)" | grep -oE "(down|up)"`
          LINK_SPEED=`swconfig dev switch0 port $p get link | grep -oE "speed:(\d+)" | grep -oE "\d+"`

          if [ "$LINK_STATUS" = "up" ]; then
               echo -e "${SWNAME}port$p\t${LINK_SPEED}\t${SECONDS}"
          fi
     done
fi
