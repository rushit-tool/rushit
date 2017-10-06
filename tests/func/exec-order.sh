#!/bin/bash

set -o errexit

basedir=$(dirname "$0")
script=$basedir/exec-order.lua
server_out=$(mktemp)
client_out=$(mktemp)

dump_logs() {
	echo "=== server process error log ==="
	cat $server_out
	echo "=== client process error log ==="
	cat $client_out
}

cleanup() {
	dump_logs
	rm $server_out $client_out
}

trap cleanup EXIT

./dummy_test --script $script > /dev/null 2> $server_out &
server_pid=$!

./dummy_test --script $script --client --test-length 1 > /dev/null 2> $client_out &
client_pid=$!

wait $client_pid
wait $server_pid

cmp $basedir/exec-order.out $server_out
cmp $basedir/exec-order.out $client_out

dump_logs() { :; }
