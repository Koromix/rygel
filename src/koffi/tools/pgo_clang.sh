#!/bin/sh -e

cd "$(dirname $0)/.."

PGO_DIR="$(pwd)/../../bin/Koffi/pgo/clang"

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
export LLVM_PROFDATA="${LLVM_PROFDATA:-llvm-profdata}"

rm -rf "$PGO_DIR"
mkdir -p "$PGO_DIR"

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_GENERATE=$PGO_DIR" --release -v

node ../cnoke/cnoke.js -D benchmark --release -v
node ../cnoke/cnoke.js -D test --release -v

echo "Running tests and benchmarks to generate profile data..."
node benchmark/rand.js koffi >/dev/null
node benchmark/atoi.js koffi >/dev/null
node benchmark/memset.js koffi >/dev/null
node benchmark/qsort.js koffi >/dev/null
node benchmark/raylib.js koffi >/dev/null
for x in {1..100}; do
    node test/sync.js >/dev/null
done
for x in {1..100}; do
    node test/callbacks.js --no_poll >/dev/null
done

$LLVM_PROFDATA merge -output="$PGO_DIR/koffi.profdata" "$PGO_DIR/"*.profraw

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_PROFILE=$PGO_DIR/koffi.profdata" --release -v
