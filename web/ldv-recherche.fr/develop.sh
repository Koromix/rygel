#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast nestor hodler

trap 'kill $(jobs -p) 2>/dev/null' EXIT
trap 'kill $(jobs -p) 2>/dev/null' SIGINT

mkdir -p dist
../../bin/Fast/nestor dist/ &
../../bin/Fast/hodler . -O dist --loop --sourcemap &

wait $(jobs -p)
