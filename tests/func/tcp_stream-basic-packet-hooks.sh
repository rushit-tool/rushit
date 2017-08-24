#!/bin/sh

set -o errexit

basedir=$(dirname "$0")
topdir=${basedir}/../..

workload=${topdir}/tcp_stream
script=${basedir}/basic-packet-hooks.lua
options="--script ${script} --test-length 1"

${workload} ${options} > /dev/null &
server_pid=$!

${workload} --client ${options} > /dev/null &
client_pid=$!

wait $client_pid
wait $server_pid
