#!/bin/sh -e

cd "$(dirname $0)/.."

PGO_DIR="$(pwd)/../../bin/Koffi/pgo"

rm -rf "$PGO_DIR"
mkdir -p "$PGO_DIR"

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_GENERATE=$PGO_DIR" --clang --release -v

node ../cnoke/cnoke.js -D test --release --clang -v
node test/test.js
node test/test.js

llvm-profdata merge -output="$PGO_DIR/koffi.profdata" "$PGO_DIR/"*.profraw

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_PROFILE=$PGO_DIR/koffi.profdata" --clang --release -v
