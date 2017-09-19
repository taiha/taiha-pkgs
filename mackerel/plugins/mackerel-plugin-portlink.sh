#!/bin/sh
# ref: https://github.com/ymyzk/mackerel-plugins/blob/master/raspberrypi/temp.sh
SECONDS=`date '+%s'`

SWNAME='switch0.'
PORTS=`for i in $(seq 0 5) ;do uci -q get network.@switch_vlan[$i].ports ; done | grep -oE "[0-9][^t]"`

for p in $PORTS; do
     NAME=${SWNAME}port$p
     LINK_STATUS=`swconfig dev switch0 port $p get link | grep -oE "link:(\w+)" | grep -oE "(down|up)"`
     LINK_SPEED=`swconfig dev switch0 port $p get link | grep -oE "speed:(\d+)" | grep -oE "\d+"`

     if [ "$LINK_STATUS" = "up" ]; then
          echo -e "${SWNAME}port$p\t${LINK_SPEED}\t${SECONDS}"
     fi
done
#echo -e "${NAME}\t${VALUE}\t${SECONDS}"
