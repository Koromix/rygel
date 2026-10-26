#!/bin/sh -e

cd "$(dirname $0)/.."

PGO_DIR="$(pwd)/../../bin/Koffi/pgo/clang"

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
if [ -z "$LLVM_PROFDATA" ]; then
    if command -v llvm-profdata >/dev/null; then
        export LLVM_PROFDATA=llvm-profdata
    elif command -v xcrun >/dev/null; then
        export LLVM_PROFDATA="xcrun llvm-profdata"
    else
        # Won't work but will make for clear failure
        export LLVM_PROFDATA=llvm-profdata
    fi
fi

rm -rf "$PGO_DIR"
mkdir -p "$PGO_DIR"

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_GENERATE=$PGO_DIR" --release -v

node ../cnoke/cnoke.js -D benchmark --release -v
node ../cnoke/cnoke.js -D test --release -v

echo "Running tests and benchmarks to generate profile data..."
node benchmark/rand.js Koffi >/dev/null
node benchmark/atoi.js Koffi >/dev/null
node benchmark/memset.js Koffi >/dev/null
node benchmark/qsort.js Koffi >/dev/null
node benchmark/raylib.js Koffi >/dev/null
for x in {1..100}; do
    node test/sync.js --no_slow >/dev/null
done
for x in {1..100}; do
    node test/async.js --no_poll >/dev/null
done
for x in {1..100}; do
    node test/callbacks.js --no_poll >/dev/null
done

$LLVM_PROFDATA merge -output="$PGO_DIR/koffi.profdata" "$PGO_DIR/"*.profraw

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_PROFILE=$PGO_DIR/koffi.profdata" --release -v
