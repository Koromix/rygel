#!/bin/sh

set -e
cd $(dirname $0)

SRC=$(ls -1 ../felix/*.cc ../core/base/base.cc ../core/compress/miniz.cc ../core/wrap/json.cc \
            ../../vendor/miniz/miniz.c ../../vendor/pugixml/src/pugixml.cpp)

PRESET=Fast
TEMP=../../bin/BootstrapFelix
BUILD=../../bin/$PRESET
BINARY=../../felix

if uname | grep -qi mingw; then
    LIBRARIES="-lws2_32 -ladvapi32 -lshell32 -lole32 -luuid"
else
    LIBRARIES=""
fi

if command -v $BINARY >/dev/null 2>&1; then
    $BINARY -p$PRESET felix $* && $BUILD/felix -q -p$PRESET felix $* && ln -sf bin/$PRESET/felix $BINARY && exit
    rm -f $BINARY
fi

if command -v clang++ >/dev/null 2>&1; then
    echo "Bootstrapping felix with Clang..."
    mkdir -p $TEMP
    clang++ -std=gnu++20 -O0 -I../.. -DNDEBUG $SRC -Wno-everything -pthread $LIBRARIES -o $TEMP/felix
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
    g++ -std=gnu++20 -O0 -I../.. -DNDEBUG $SRC -w -pthread $LIBRARIES -o $TEMP/felix
    $TEMP/felix -p$PRESET felix $*
    ln -sf bin/$PRESET/felix $BINARY

    echo "Cleaning up..."
    rm -f $TEMP/*
    rmdir $TEMP

    exit
fi

echo "Could not find any compiler (g++, clang++) in PATH"
exit 1
