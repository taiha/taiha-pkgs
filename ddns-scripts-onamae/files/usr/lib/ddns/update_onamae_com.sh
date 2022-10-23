# This is a sub-package for ddns-scripts in OpenWrt to update
# IPv4 address of onamae.com.
#
# following options are required:
#	- username
#	- password
#	- domain
#
# refs:
#   https://qiita.com/ats124/items/59ec0f444d00bbcea27d
#   https://koriyoukai.net/blog/index.php/20191207_264
#   https://www.harada-its.com/2019/06/01-421/

if type openssl &> /dev/null; then
	"Performing update for onamae.com requires openssl (package: openssl-util). Please install"
fi
[ -z "{$username}" ] && write_log 14 "Service section not configured correctly! Missing key as 'username'"
[ -z "${password}" ] && write_log 14 "Service section not configured correctly! Missing secret as 'password'"
[ -z "${domain}" ] && write_log 14 "Service section not configured correctly! Missing zone id as 'domain'"

local ENDPOINT="ddnsclient.onamae.com"
local ENDPOINT_PORT="65010"

local DOMAIN_TLD DOMAIN_NOTLD DOMAIN_SCND DOMAIN_SUB
							# test.www.example.com
DOMAIN_TLD="${domain##*.}"				# TLD of the specified domain ("com")
DOMAIN_NOTLD="${domain%%.${DOMAIN_TLD}}"		# the specified domain without TLD ("test.www.example")
DOMAIN_SCND="${DOMAIN_NOTLD##*.}"			# the second level domain of ("example")
[ -z "${DOMAIN_SCND}" ] && write_log 14 "Invalid second level domain name is specified"
DOMAIN_SUB="${domain%%${DOMAIN_SCND}.${DOMAIN_TLD}}"	# least sub-domains of the specified domain ("test.www.")
DOMAIN_SUB="${DOMAIN_SUB%%.}"				# remove a comma at the end of sub-domains

# comma at the end of each block is required to
# submit & execute a command on server
local DDNS_TEXT

DDNS_TEXT="LOGIN
USERID:${username}
PASSWORD:${password}
.
MODIP"
if [ -n "${DOMAIN_SUB}" ]; then
	DDNS_TEXT="${DDNS_TEXT}
HOSTNAME:${DOMAIN_SUB}"	# add "HOSTNAME" if sub-domains is supecified
fi
DDNS_TEXT="${DDNS_TEXT}
DOMNAME:${DOMAIN_SCND}.${DOMAIN_TLD}
IPV4:${__IP}
.
LOGOUT
."

local CMD_RESULT_CNT ERROR_MSG
ANSWER="$(echo "${DDNS_TEXT}" |\
		flock /tmp/$(basename -s .sh "$0").lock \
		openssl s_client --connect \
		${ENDPOINT}:${ENDPOINT_PORT} \
		-quiet 2>/dev/null)"

# search result messages (ex: "000 COMMAND SUCCESSFUL", "002 LOGIN ERROR")
ANSWER="$(echo "${ANSWER}" | grep -E "[0-9]{3}\s")"

# count result messages of successful
CMD_RESULT_CNT="$(echo "${ANSWER}" | grep -c "000 COMMAND SUCCESSFUL")"

# error occured when the count is smaller than 4
if [ "$CMD_RESULT_CNT" -lt "4" ]; then
	ERROR_MSG="$(echo "${ANSWER}" | grep -v "000 COMMAND SUCCESSFUL")"

	write_log 4 "Failed to update IP address on onamae.com (domain: ${domain})"
	if [ -n "$ERROR_MSG" ]; then	# error message is printed
		write_log 17 "${ERROR_MSG}"	# write log message with error massage from server and exit
	else				# error message isn't printed (ex.: address contains invalid character(s))
		write_log 17 "(The server (${ENDPOINT}) didn't reply any error messages, unknown error)"
	fi
else
	write_log 5 "Succeeded to update IP address on onamae.com (domain: ${domain})"
fi

return 0
