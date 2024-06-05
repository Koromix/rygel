#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast serf hodler

trap 'kill $(jobs -p) 2>/dev/null' EXIT
trap 'kill $(jobs -p) 2>/dev/null' SIGINT

../../bin/Fast/serf dist/ &
../../bin/Fast/hodler . -O dist --loop &

wait $(jobs -p)
