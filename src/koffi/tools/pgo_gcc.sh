#!/bin/sh -e

cd "$(dirname $0)/.."

PGO_DIR="$(pwd)/../../bin/Koffi/pgo/gcc"

export CC="${CC:-gcc}"
export CXX="${CXX:-g++}"

rm -rf "$PGO_DIR"
mkdir -p "$PGO_DIR"

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_GENERATE=$PGO_DIR" --release -v

node ../cnoke/cnoke.js -D benchmark --release -v
node ../cnoke/cnoke.js -D test --release -v

echo "Running tests and benchmarks to generate profile data..."
node benchmark/benchmark.js --koffi >/dev/null
for x in {1..100}; do
    node test/sync.js >/dev/null
done
for x in {1..100}; do
    node test/callbacks.js --no_poll >/dev/null
done

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_PROFILE=$PGO_DIR/" --release -v
