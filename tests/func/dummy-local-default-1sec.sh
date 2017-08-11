#!/bin/sh

set -o errexit

./dummy_test > /dev/null &
server_pid=$!

./dummy_test --client > /dev/null &
client_pid=$!

wait $client_pid
wait $server_pid
