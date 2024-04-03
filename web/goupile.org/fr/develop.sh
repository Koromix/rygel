#!/bin/bash -e

cd "$(dirname $0)"

trap 'kill $(jobs -p)' EXIT

../../../bootstrap.sh

../../../felix -pFast serf hodler
../../../bin/Fast/hodler . -O dist
../../../bin/Fast/serf &
sleep 10

watch -n1 ../../../felix -qq -pFast --run_here hodler . -O dist
