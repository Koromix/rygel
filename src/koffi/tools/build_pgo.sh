#!/bin/sh -e

cd "$(dirname $0)/.."

PGO_DIR="$(pwd)/../../bin/Koffi/pgo"

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
export LLVM_PROFDATA="${LLVM_PROFDATA:-llvm-profdata}"

rm -rf "$PGO_DIR"
mkdir -p "$PGO_DIR"

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_GENERATE=$PGO_DIR" --release -v

node ../cnoke/cnoke.js -D test --release -v
node test/test.js
node test/test.js

$LLVM_PROFDATA merge -output="$PGO_DIR/koffi.profdata" "$PGO_DIR/"*.profraw

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_PROFILE=$PGO_DIR/koffi.profdata" --release -v
