#!/bin/sh

set -o errexit

basedir=$(dirname "$0")
script=$basedir/get-so-type.lua

./dummy_test --script $script > /dev/null &
server_pid=$!

./dummy_test --script $script --client > /dev/null &
client_pid=$!

wait $client_pid
wait $server_pid
