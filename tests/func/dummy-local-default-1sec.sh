#!/bin/sh

set -o errexit

./dummy_test --logtostderr &
server_pid=$!

./dummy_test --logtostderr --test-length 1 --client &
client_pid=$!

wait $client_pid
wait $server_pid
