#!/bin/bash
#
# Run a set of udp_stream tests over loopback to exercise all
# available command line flags. Check for non-zero exit status.
#

set -o errexit

basedir="$(dirname "$0")"
topdir="${basedir}/../.."

PATH="${basedir}:${topdir}"

[ -x "$(type -P test-run)" ] || {
	echo 2>&1 "ERROR: Test runner ('test-run') missing!"
	exit 1
}

fixed_opts="--test-length 1"

server_opts=
client_opts=
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--magic 73"
client_opts="--magic 73"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--maxevents 10"
client_opts="--maxevents 100"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts=""
client_opts="--num-flows 10"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts=""
client_opts="--num-flows 16 --num-threads 4"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--buffer-size 4096"
client_opts="--buffer-size 8192"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--ipv4"
client_opts="--ipv4"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--ipv6"
client_opts="--ipv6"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--dry-run"
client_opts="--dry-run"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--nonblocking"
client_opts="--nonblocking"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--interval 0.5"
client_opts="--interval 2.0"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts=""
client_opts="--local-host localhost"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts=""
client_opts="--host localhost"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--control-port 12345"
client_opts="--control-port 12345"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--port 54321"
client_opts="--port 54321"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--all-samples=/dev/null"
client_opts="--all-samples=/dev/null"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}

server_opts="--num-threads 2 --reuseport"
client_opts="--num-threads 2 --num-flows 2 --reuseport"
test-run udp_stream ${server_opts} -- ${client_opts} ${fixed_opts}
