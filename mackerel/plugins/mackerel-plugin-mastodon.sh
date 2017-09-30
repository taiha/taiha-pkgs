#!/bin/sh
SECONDS=`date '+%s'`

PLUGIN_NAME='mastodon'
GRAPH_PREFIX="# mackerel-agent-plugin\n"
GRAPH_DEF="{\"graphs\":{"
GRAPH_DEF="${GRAPH_DEF}\"mastodon.Users.#\":{\"label\":\"Mastodon Statistics (Users)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":false}]},"
GRAPH_DEF="${GRAPH_DEF}\"mastodon.Toots.#\":{\"label\":\"Mastodon Statistics (Toots)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":false}]},"
GRAPH_DEF="${GRAPH_DEF}\"mastodon.Instances.#\":{\"label\":\"Mastodon Statistics (Instances)\",\"unit\":\"integer\",\"metrics\":[{\"name\":\"*\",\"label\":\"%1\",\"stacked\":false}]}"
GRAPH_DEF="${GRAPH_DEF}}}"

if [ -n "$MACKEREL_AGENT_PLUGIN_META" ]; then
	echo -e ${GRAPH_PREFIX}${GRAPH_DEF}
	exit 0
fi
# Load json tool
. /usr/share/libubox/jshn.sh

while getopts d: option; do
	case $option in
		d)
			HOSTS=$OPTARG;;
		\?)
			echo "Usage: $0 [-h domain,domain,...]" 1>&2
			exit 1;;
	esac
done

IFS=","
for host in $HOSTS; do
	toots='0'
	users='0'
	instances='0'

	# Load json from /api/v1/instance (>= v1.6.0)
	MSTDN_JSON=$(curl -sS https://${host}/api/v1/instance)
	json_load "$MSTDN_JSON"

	# Get value in uri and title
#	json_get_var domain uri		# instance domain name (uri)
	json_get_var title title	# instance title
	# Select "stats" field
	json_select stats
	# Get values in stats
	json_get_var user_cnt user_count	# Users count
	json_get_var toot_cnt status_count	# Statuses (Toots) count
	json_get_var domain_cnt domain_count	# Domains count

	host=$(echo $host | tr "." "_")
	echo -e "${PLUGIN_NAME}.Users.${host}.users\t${user_cnt}\t${SECONDS}"
	echo -e "${PLUGIN_NAME}.Toots.${host}.toots\t${toot_cnt}\t${SECONDS}"
	echo -e "${PLUGIN_NAME}.Instances.${host}.instances\t${domain_cnt}\t${SECONDS}"
done
