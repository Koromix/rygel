#/bin/sh

cd $(dirname $0)

SRC=$(ls -1 ../../src/felix/*.cc ../../src/libcc/libcc.cc ../../src/libwrap/json.cc ../../vendor/miniz/miniz.c)
BIN=../../felix

if command -v g++ >/dev/null 2>&1; then
    echo "Building felix with GCC"
    g++ -std=gnu++17 -O2 -DNDEBUG $SRC -ldl -pthread -o $BIN
    exit
fi

if command -v clang++ >/dev/null 2>&1; then
    echo "Building felix with Clang"
    clang++ -std=gnu++17 -O2 -DNDEBUG $SRC -ldl -pthread -o $BIN
    exit
fi

echo "Could not find any compiler (g++, clang++) in PATH"
