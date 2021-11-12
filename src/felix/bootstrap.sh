#!/bin/sh -e

cd $(dirname $0)

SRC=$(ls -1 ../../src/felix/*.cc ../../src/core/libcc/libcc.cc ../../src/core/libwrap/json.cc ../../vendor/miniz/miniz.c)
BIN=../../felix
BUILD=../../bin/BootstrapFelix

if command -v clang++ >/dev/null 2>&1; then
    echo "Bootstrapping felix with Clang..."
    mkdir -p $BUILD
    clang++ -std=gnu++2a -O0 -DNDEBUG -DLIBCC_NO_BROTLI $SRC -Wno-everything -ldl -pthread -o $BUILD/felix
    $BUILD/felix --no_presets --features=OptimizeSpeed -O $BUILD/Fast felix
    mv $BUILD/Fast/felix $BIN

    echo "Cleaning up..."
    rm -rf $BUILD/Fast
    rm -f $BUILD/*
    rmdir $BUILD

    exit
fi

if command -v g++ >/dev/null 2>&1; then
    echo "Bootstrapping felix with GCC..."
    mkdir -p $BUILD
    g++ -std=gnu++2a -O0 -DNDEBUG -DLIBCC_NO_BROTLI $SRC -w -ldl -pthread -o $BUILD/felix
    $BUILD/felix --no_presets --features=OptimizeSpeed -O $BUILD/Fast felix
    mv $BUILD/Fast/felix $BIN

    echo "Cleaning up..."
    rm -rf $BUILD/Fast
    rm -f $BUILD/*
    rmdir $BUILD

    exit
fi

echo "Could not find any compiler (g++, clang++) in PATH"
