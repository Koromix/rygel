#!/bin/bash -e

cd "$(dirname $0)"

trap 'kill $(jobs -p)' EXIT

../../../bootstrap.sh

../../../felix -q -pFast serf hodler
../../../bin/Fast/hodler . -O dist
../../../bin/Fast/serf &
sleep 10

watch -n1 ../../../felix -q -pFast --run_here hodler . -O dist
