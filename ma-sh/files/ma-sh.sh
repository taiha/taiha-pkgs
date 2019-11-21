#!/bin/sh

MA_AGENT_DEF_NAME="ma-shT"
MA_AGENT_DEF_VER="0.0.0"

# program settings
MA_PID_FILE="/var/run/ma-sh.pid"
MA_POST_INT="60"
MA_SUPPOTED_STATS="working standby maintenance poweroff"
MA_ECHO_NODATE="1"
MA_TOOL="/usr/sbin/ma-tools"

# debug
DEBUG="0"
DEBUG_CURL="0"		# output curl informations verbosely for debugging
LOCAL_DEBUG="0"	# do not access to Mackerel API, local test only

# api settings
CONFIG_USE_MODEL=
CONFIG_EXIT_STAT=
CONFIG_APIBASE=
CONFIG_APIKEY=
CONFIG_HOSTID=
CONFIG_TIMEOUT=

# parameters
PARAM_DAEMON=
PARAM_SYSLOG_OUTPUT=

# tmp system status json

func_print_log() {
	local priority="$1" \
		msg="$2"

	[ "$priority" = "debug" -a "$DEBUG" = "0" ] && return
	if [ "$PARAM_SYSLOG_OUTPUT" = "1" ]; then
		logger -p daemon."$priority" -t "${MA_AGENT_NAME:-MA_AGENT_DEF_NAME}" "$msg"
	else
		[ ! "$MA_ECHO_NODATE" ] && echo -n "$(date): "
		echo "$priority: $msg"
	fi
}

func_dump() {
	echo -e "==== $1 ===="
	echo "$2"
	echo "=============="
}

TMP_RESPONSE="/tmp/mackerel_res.txt"

# Build API url
# params: category, api_path
func_build_url() {
	local category="${1}"
	local api_path="${2}"
	local base_url="https://${CONFIG_APIBASE}/api/v0"
	local url

	case "${category}" in
		reg)
			url="${base_url}/hosts"
			;;
		graphdef)
			url="${base_url}/graph-defs/create"
			;;
		host)
			url="${base_url}/hosts/${CONFIG_HOSTID}${api_path}"
			;;
		metric)
			url="${base_url}/tsdb"
			;;
	esac

	echo $url
}

# Access to the API
# paramerters: method, data
func_access_api() {
	local method="${1}"
	local url="${2}"
	local data="${3}"
	local curl_opt

	if [ "$DEBUG_CURL" = "1" ]; then
		curl_opt="-v"
	else
		curl_opt="-s"
	fi

	if [ "$LOCAL_DEBUG" = "1" ]; then
#		echo "method: $method"
#		echo "url: $url"
#		echo -e "data:\n$data"
		echo 200
	else
		curl -s ${url} -X${method} \
			-H "X-api-key: ${CONFIG_APIKEY}" -H 'Content-Type: application/json' \
			--data "${data}" \
			-o "$TMP_RESPONSE" -w %{http_code} -m "${CONFIG_TIMEOUT:-10}"
	fi
}

func_http_check() {
	local http="$1"

	# success
	if [ "$http" = "200" ]; then
		return 0
	fi

	# fail
	if [ "$http" = "400" ]; then
		func_print_log "err" "API: invalid format"
		return 1
	fi
	if [ "$http" = "403" ]; then
		func_print_log "err" "API: access denied"
		return 1
	fi
	if [ "$http" = "404" ]; then
		func_print_log "err" "API: not found"
		return 1
	fi

	# fail (other reason (incl. timeout), http=000?)
	func_print_log "err" "API: failed by any reason"
	return 1
}

func_host_regupd() {
	local category=${1:-"reg"}	# reg: POST, host (upd): PUT
	local method
	local url
	local json
	local http

	json="$($MA_TOOL ${CONFIG_USE_MODEL:+-m} systemj)"
#	func_dump "json" "$json"
	url="$(func_build_url $category)"
	[ "$category" = "reg" ] && method="POST" || method="PUT"
	http="$(func_access_api "$method" "$url" "$json")"
	func_print_log "debug" "register or update result for host information (url: $url, http: $http)"

	if ! func_http_check "$http"; then
		return 1
	fi
	if [ "$category" = "host" ]; then
			return 0
	fi
	CONFIG_HOSTID="$(jsonfilter -s "$TMP_RESPONSE" -e '@.id')"
	uci_set "ma-sh" "global" "hostid" "$CONFIG_HOSTID"
	uci_commit "ma-sh"

	return 0
#	func_dump "ret" "$ret"
}

build_status_json() {
	local status="$1"

	json_init
	json_add_string "status" "$status"
}

# status: working, standby, maintenance, poweroff
func_status_upd() {
	local status="$1"
	local http

	build_status_json "$status"
	json="$(json_dump)"
#	func_dump "json" "$json"
	url="$(func_build_url host '/status')"
	http="$(func_access_api POST "$url" "$json")"
	func_print_log "debug" "updating result for host status (url: $url, http: $http)"

	if ! func_http_check "$http"; then
		func_print_log "err" "failed to update the host status, exit..."
		return 1
	fi

	func_print_log "notice" \
		"succeeded to update host status (new status: $status)"

	return 0
#	func_dump "ret" "$ret"
}

func_metric_post() {
	local time=$(date '+%s')
	local data
	local json
	local url
	local http

	json="$($MA_TOOL -h "$CONFIG_HOSTID" metricj)"
	[ "$DEBUG" = "1" ] && func_dump "json" "$json"

	url="$(func_build_url 'metric')"
	http="$(func_access_api "POST" $url "$json")"
	func_print_log "debug" "posting result for metric (url: $url, http: $http)"

	if ! func_http_check "$http"; then
		func_print_log "warn" "failed to post the host metric"
	else
		func_print_log "debug" "succeeded to post host metric"
	fi

	return 0
}

func_agent_exit() {
	func_print_log "notice" "signal recieved, start shutdown..."

	if ! func_status_upd "$CONFIG_EXIT_STAT"; then
		func_print_log "warn" "failed to update the host status, please update manually"
	fi
	exit
}

func_main() {
	local regupd_action="reg"
	local ret

	[ ! -d "$MA_TMP_DIR" ] && mkdir "$MA_TMP_DIR"
	if [ -n "$CONFIG_HOSTID" ]; then
		regupd_action="host"
	fi

	if ! func_host_regupd "$regupd_action"; then
		func_print_log "err" "failed to register or update the host information, exit..."
		exit 1
	else
		func_print_log "notice" "succeeded to register or update the host information (host id: $CONFIG_HOSTID)"
	fi
	if [ "$regupd_action" = "host" ]; then
		if ! func_status_upd "working"; then
			exit 1
		fi
	fi

	CNT=
	while true; do
		TIME=$(date '+%s') #&& echo "TIME: $TIME"
		SEC=$((TIME % MA_POST_INT))
		if [ "$SEC" != "0" ]; then
			WAIT=$((MA_POST_INT - SEC))
			sleep $WAIT &
			wait $!
			#echo "adjust: ${WAIT}s"
		fi
		func_metric_post &
		#echo "CNT: $CNT"
		CNT=$((CNT + 1))
		sleep $((MA_POST_INT - 2)) &
		wait $!
#		[ "$CNT" = "5" ] && break
	done
}

if ! type curl &> /dev/null; then
	func_print_log "err" "\"curl\" command not found, please install it"
	exit 1
fi

if [ -r "/lib/functions.sh" ] && [ -r "/usr/share/libubox/jshn.sh" ]; then
	. "/lib/functions.sh"
	. "/usr/share/libubox/jshn.sh"
	func_print_log "debug" "libraries are loaded, starting..."
else
	func_print_log "err" "failed to load libraries"
	exit 1
fi

# Sub command
agent_cmd="${1:-"start"}"
shift

# Parameters
while getopts CdDS option; do
	case $option in
		C)
			DEBUG_CURL="1"
			;;
		d)
			PARAM_DAEMON="1"
			;;
		D)
			DEBUG="1"
			;;
		S)
			PARAM_SYSLOG_OUTPUT="1"
			;;
	esac
done

#func_print_log "info" "PARAM_DAEMON: $PARAM_DAEMON"
config_load "ma-sh"
config_get CONFIG_USE_MODEL "global" "use_model"
config_get CONFIG_EXIT_STAT "global" "exit_stat"
config_get CONFIG_APIBASE "global" "apibase"
config_get CONFIG_APIKEY "global" "apikey"
config_get CONFIG_HOSTID "global" "hostid"
config_get CONFIG_TIMEOUT "global" "timeout"

if [ "$PARAM_SYSLOG_OUTPUT" != "1" -o "$PARAM_DAEMON" = "1" ]; then
	func_print_log "info" "${MA_AGENT_NAME:-MA_AGENT_DEF_NAME} ${MA_AGENT_VER:-MA_AGENT_DEF_VER}"
	func_print_log "info" "API base: $CONFIG_APIBASE"
fi
if [ -z "$CONFIG_APIKEY" ]; then	# check if the api key is set
	func_print_log "err" "Please set the API key before starting this program."
	exit 1
fi
[ "$CONFIG_USE_MODEL" != "1" ] && unset CONFIG_USE_MODEL

# Operations
case "$agent_cmd" in
	start)
		if [ "$PARAM_DAEMON" != "1" -a -s "$MA_PID_FILE" ]; then	# check if agent is running
			func_print_log "err" "failed to start ${MA_AGENT_NAME:-MA_AGENT_DEF_NAME}, already running"
			exit 1
		fi

		VALID_STAT=
		for stat in $MA_SUPPOTED_STATS; do
			[ "$CONFIG_EXIT_STAT" = "$stat" ] && VALID_STAT=1 && break
		done

		if [ "$VALID_STAT" != "1" ]; then		# check if specified exit status is valid
			func_print_log "warn" "invalid status is specified in exit_stat, use \"poweroff\""
			CONFIG_EXIT_STAT="poweroff"
		fi

		trap 'func_agent_exit' 2 15
		func_print_log "notice" "start ${MA_AGENT_NAME:-MA_AGENT_DEF_NAME} agent"
		func_main
		;;
	working|\
	standby|\
	maintenance|\
	poweroff)
		if [ -z "$CONFIG_HOSTID" ]; then
			func_print_log "err" "the host id is not set"
			exit 1
		fi
		if ! func_status_upd "$agent_cmd"; then
			exit 1
		fi
		;;
	update)
		if [ -z "$CONFIG_HOSTID" ]; then
			func_print_log "err" "the host id is not set"
			exit 1
		fi
		if ! func_host_regupd "host"; then
			func_print_log "err" "failed to update the host information"
			exit 1
		fi
		func_print_log "info" "succeeded to update the host information (host id: $CONFIG_HOSTID)"
		;;
	# for debugging
	debug)
		MA_POST_INT="10"
		LOCAL_DEBUG="1"
		CONFIG_USE_MODEL="1"
		func_host_regupd host
		;;
	*)
		func_print_log "err" "no or invalid sub command is specified"
		exit 1
		;;
esac
