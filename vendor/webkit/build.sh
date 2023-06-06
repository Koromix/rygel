#!/bin/sh -e

COMMIT=WebKit-7616.1.14.11.1

cd "$(dirname $0)"

PLATFORM=$(felix --version 2| awk -F "/" "/Host/ { print \$NF }")
ARCHITECTURE=$(felix --version 2| awk -F ": " "/Architecture/ { print \$NF }")
BINARY=../lib/${PLATFORM}_${ARCHITECTURE}

rm -rf webkit build include $BINARY

git clone --branch $COMMIT --depth 1 https://github.com/WebKit/WebKit.git webkit

mkdir build && cd build
cmake -G Ninja -DPORT=JSCOnly -DCMAKE_BUILD_TYPE=Release -DDEVELOPER_MODE=OFF -DENABLE_FTL_JIT=ON -DENABLE_STATIC_JSC=ON -DUSE_THIN_ARCHIVES=OFF ../webkit
ninja

mkdir -p ../include $BINARY

cp -r JavaScriptCore/Headers/* ../include/
cp -r WTF/Headers/* ../include/
cp -r bmalloc/Headers/* ../include/
cp lib/*.a $BINARY/
