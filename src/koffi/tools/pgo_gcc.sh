#!/bin/sh -e

cd "$(dirname $0)/.."

PGO_DIR="$(pwd)/../../bin/Koffi/pgo/gcc"

export CC="${CC:-gcc}"
export CXX="${CXX:-g++}"

rm -rf "$PGO_DIR"
mkdir -p "$PGO_DIR"

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_GENERATE=$PGO_DIR" --release -v

node ../cnoke/cnoke.js -D test --release -v
node test/test.js
node test/test.js

node ../cnoke/cnoke.js clean --release
node ../cnoke/cnoke.js -d "PGO_PROFILE=$PGO_DIR/" --release -v
