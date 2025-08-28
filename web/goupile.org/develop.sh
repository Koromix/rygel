#!/bin/bash -e

cd "$(dirname $0)"

../../bootstrap.sh
../../felix -pFast nestor hodler

trap 'kill $(jobs -p) 2>/dev/null' EXIT
trap 'kill $(jobs -p) 2>/dev/null' SIGINT

mkdir -p dist
../../bin/Fast/nestor dist/ &
../../bin/Fast/hodler fr -O dist --loop --sourcemap &
../../bin/Fast/hodler en -b /en/ -O dist/en --loop --sourcemap &

wait $(jobs -p)
