#!/bin/sh
SECONDS=`date '+%s'`

PLUGIN_NAME='appproto.'

PROTO="ICMP IGMP FTP SSH Telnet SMTP DNS HTTP POP2 POP3 NTP NetBIOS IMAP IMAP3"
PROTO="${PROTO} HTTPS SMBoverTCP FTPS IMAPS POP3S BitTorrent GRE ESP IPIP L2TPv3 null"
for app in $PROTO; do
	eval rxBytes_"${app}"='0'
	eval rxPkts_"${app}"='0'
	eval txBytes_"${app}"='0'
	eval txPkts_"${app}"='0'
done

NLBW_TEXT=$(nlbw -c json | sed 's/\"data\":\[/@/' | sed 's/\],\[/\]*\[/g' )

IFS='@'
set -- $NLBW_TEXT

DATA_STR=$2
IFS='*'
i=0

for data in $DATA_STR; do
	DATA_ROW=$(echo $data | tr -d '[]}\"')

	IFS=','
	set -- $DATA_ROW
	nlbw_proto=$(echo $11 | tr -d '-')
	rx_bytes=$7
	rx_pkts=$8
	tx_bytes=$9
	tx_pkts=$10
	IFS=' '
	for app in $PROTO; do
		if [ "$app" = "$nlbw_proto" ]; then
		eval rxBytes_${app}=$(( rxBytes_${app} + $rx_bytes))
		eval rxPkts_${app}=$(( rxPkts_${app} + $rx_pkts))
		eval txBytes_${app}=$(( txBytes_${app} + $tx_bytes))
		eval txPkts_${app}=$(( txPkts_${app} + $tx_pkts))
		fi
	done
done

for app in $PROTO; do
	rxBytes=$(eval echo \"\$rxBytes_${app}\")
	rxPkts=$(eval echo \"\$rxPkts_$app\")
	txBytes=$(eval echo \"\$txBytes_$app\")
	txPkts=$(eval echo \"\$txPkts_$app\")
	if [ "$app" = "null" ]; then
		app="Other"
	fi
	if [ "$rxBytes" != "0" ] || [ "$rxPkts" != "0" ] || [ "$txBytes" != "0" ] || [ "$txPkts" != "0" ] ; then
		echo -e "${PLUGIN_NAME}${app}.rxBytes\t${rxBytes}\t${SECONDS}"
		echo -e "${PLUGIN_NAME}${app}.rxBytes\t${rxPkts}\t${SECONDS}"
		echo -e "${PLUGIN_NAME}${app}.txBytes\t${txBytes}\t${SECONDS}"
		echo -e "${PLUGIN_NAME}${app}.txBytes\t${txPkts}\t${SECONDS}"
	fi
done

