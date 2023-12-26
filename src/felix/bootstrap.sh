#!/bin/sh -e

cd $(dirname $0)

SRC=$(ls -1 ../felix/*.cc ../core/libcc/libcc.cc ../core/libwrap/json.cc ../../vendor/pugixml/src/pugixml.cpp)

PRESET=Fast
TEMP=../../bin/BootstrapFelix
BUILD=../../bin/$PRESET
BINARY=../../felix

if command -v $BINARY >/dev/null 2>&1; then
    $BINARY -p$PRESET felix $* && ln -sf bin/$PRESET/felix $BINARY && exit
    rm -f $BINARY
fi

if command -v clang++ >/dev/null 2>&1; then
    echo "Bootstrapping felix with Clang..."
    mkdir -p $TEMP
    clang++ -std=gnu++17 -O0 -I../.. -DNDEBUG $SRC -Wno-everything -pthread -o $TEMP/felix
    $TEMP/felix -p$PRESET felix $*
    ln -sf bin/$PRESET/felix $BINARY

    echo "Cleaning up..."
    rm -f $TEMP/*
    rmdir $TEMP

    exit
fi

if command -v g++ >/dev/null 2>&1; then
    echo "Bootstrapping felix with GCC..."
    mkdir -p $TEMP
    g++ -std=gnu++17 -O0 -I../.. -DNDEBUG $SRC -w -pthread -o $TEMP/felix
    $TEMP/felix -p$PRESET felix $*
    ln -sf bin/$PRESET/felix $BINARY

    echo "Cleaning up..."
    rm -f $TEMP/*
    rmdir $TEMP

    exit
fi

echo "Could not find any compiler (g++, clang++) in PATH"
exit 1
