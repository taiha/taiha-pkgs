#!/bin/sh
# ref: https://github.com/ymyzk/mackerel-plugins/blob/master/raspberrypi/temp.sh
SECONDS=`date '+%s'`

NAME='temperature.cpu'
TEMP=`cat /sys/class/thermal/thermal_zone0/temp`
VALUE=$((${TEMP} / 1000))
echo -e "${NAME}\t${VALUE}\t${SECONDS}"
