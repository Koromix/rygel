#!/bin/sh

cd $(dirname $0)

SRC=$(ls -1 ../../src/felix/*.cc ../../src/core/libcc/libcc.cc ../../src/core/libwrap/json.cc ../../vendor/miniz/miniz.c)
BIN=../../felix

if command -v g++ >/dev/null 2>&1; then
    echo "Bootstrapping felix with GCC..."
    mkdir -p tmp
    g++ -std=gnu++2a -O0 -DNDEBUG $SRC -w -ldl -pthread -o tmp/felix
    tmp/felix -m Fast -O tmp/fast felix
    mv tmp/fast/felix $BIN

    echo "Cleaning up..."
    rm -rf tmp/fast
    rm -f tmp/*
    rmdir tmp

    exit
fi

if command -v clang++ >/dev/null 2>&1; then
    echo "Bootstrapping felix with Clang..."
    mkdir -p tmp
    clang++ -std=gnu++2a -O0 -DNDEBUG $SRC -Wno-everything -ldl -pthread -o tmp/felix
    tmp/felix -m Fast -O tmp/fast felix
    mv tmp/fast/felix $BIN

    echo "Cleaning up..."
    rm -rf tmp/fast
    rm -f tmp/*
    rmdir tmp

    exit
fi

echo "Could not find any compiler (g++, clang++) in PATH"
