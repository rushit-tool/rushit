#!/bin/bash

set -o errexit

basedir=$(dirname "$0")
script=$basedir/print-hook-name.lua
server_out=$(mktemp)
client_out=$(mktemp)

./dummy_test --script $script | grep '^TRACE:'> $server_out &
server_pid=$!

./dummy_test --script $script --client | grep '^TRACE:' > $client_out &
client_pid=$!

wait $client_pid
wait $server_pid

cmp $basedir/print-hook-name.server.out $server_out
cmp $basedir/print-hook-name.client.out $client_out

rm $server_out $client_out
