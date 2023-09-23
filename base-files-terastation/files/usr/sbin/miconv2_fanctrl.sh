#!/bin/sh

#----------------------
# Variables
#----------------------
# MICON
MICON_HWMON="$(ls -d /sys/devices/platform/ts-miconv2/ts-miconv2-hwmon/hwmon/hwmon* 2>/dev/null)"

# Thermal Sensors
MICON_TSENS="$MICON_HWMON/temp1_input"
I2C_TSENS="$(ls -d /sys/class/i2c-dev/i2c-0/device/0-00*/hwmon/hwmon*/temp1_input 2>/dev/null)"
SOC_TSENS="$(ls -d /sys/devices/virtual/thermal/thermal_zone0/hwmon*/temp1_input 2>/dev/null)"
SYS_TSENS=""

# Temp-Fan mapping
# (use (5*N - 2) value for execution interval by cron)
SOCSYS_TEMP_LOW="38:28"
SOCSYS_TEMP_MID="48:38"
SOCSYS_TEMP_HIG="53:43"

# Temperatures
SOC_TEMP=""
SYS_TEMP=""

# Fan Steps
FAN_OLD_STEP=
FAN_NEW_STEP=0
FAN_STOP=0
FAN_LOW=1
FAN_MID=2
FAN_HIG=3

pr_info() {
	echo "info: $@"
}

pr_err() {
	echo "err: $@" 1>&2
}

#----------------------
# Initialize
#----------------------
#echo "MICON_TSENS: $MICON_TSENS"
#echo "I2C_TSENS  : $I2C_TSENS"
#echo "SOC_TSENS  : $SOC_TSENS"

# check availability
if [ -z "$MICON_HWMON" ]; then
	pr_err "MICON hwmon device is unavailable, exit..."
	exit 1
fi

if cat "$MICON_TSENS" 2>/dev/null; then
	SYS_TSENS="$MICON_TSENS"
elif [ -n "$I2C_TSENS" ]; then
	SYS_TSENS="$I2C_TSENS"
	#pr_info "MICON thermal sensor is unavailable, using I2C sensor"
else
	pr_err "there is no available thermal sensor for system, exit..."
	exit 1
fi

get_new_fanstep() {
	local new_step=0

	for t in $SOCSYS_TEMP_LOW $SOCSYS_TEMP_MID $SOCSYS_TEMP_HIG; do
		#pr_err "${t%%:*}, ${t##*:}"
		[ $SOC_TEMP -lt ${t%%:*} ] && [ $SYS_TEMP -lt ${t##*:} ] && \
			echo $new_step && return

		new_step=$((new_step + 1))
	done

	echo $new_step
}

#----------------------
# Check temperature and set fan
#----------------------
SOC_TEMP="$(( $(cat $SOC_TSENS) / 1000 ))" # convert to celsius from milli-
SYS_TEMP="$(( $(cat $SYS_TSENS) / 1000 ))" # convert to celsius from milli-
FAN_OLD_STEP="$(cat $MICON_HWMON/fan1_target)"

pr_info "SoC: $SOC_TEMP celsius, System: $SYS_TEMP celsius"

FAN_NEW_STEP=$(get_new_fanstep)

[ $FAN_NEW_STEP -eq $FAN_OLD_STEP ] && exit

pr_info "Fan: $FAN_OLD_STEP --> $FAN_NEW_STEP"
echo "$FAN_NEW_STEP" > "$MICON_HWMON"/fan1_target
