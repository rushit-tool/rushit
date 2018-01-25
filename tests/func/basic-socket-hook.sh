#!/bin/bash
#
# Quick run of every workload with a script that sets a socket option.
#

set -o errexit

basedir=$(dirname "$0")
topdir="${basedir}/../.."

PATH="${basedir}:${topdir}"

[ -x "$(type -P test-run)" ] || {
	echo 2>&1 "ERROR: Test runner ('test-run') missing!"
	exit 1
}

script="${basedir}/basic-socket-hook.lua"
server_opts=(--script "${script}")
client_opts=(--script "${script}" --test-length 1)

for workload in tcp_rr tcp_stream udp_stream; do
	test-run ${workload} "${server_opts[@]}" -- "${client_opts[@]}"
done
