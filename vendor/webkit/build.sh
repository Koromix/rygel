#!/bin/sh -e

COMMIT=WebKit-7616.1.14.11.1

cd "$(dirname $0)"

PLATFORM=$(felix --version 2| awk -F "/" "/Host/ { print \$NF }")
ARCHITECTURE=$(felix --version 2| awk -F ": " "/Architecture/ { print \$NF }")

rm -rf webkit build

git clone --branch $COMMIT --depth 1 https://github.com/WebKit/WebKit.git webkit

ls ../_patches/webkit_*.patch | xargs -n1 patch -Np3 -i

mkdir build && cd build
cmake -G Ninja -DPORT=JSCOnly -DCMAKE_BUILD_TYPE=Release -DDEVELOPER_MODE=OFF -DENABLE_FTL_JIT=ON -DENABLE_STATIC_JSC=ON -DUSE_THIN_ARCHIVES=OFF ../webkit
ninja

BINARY=../lib/${PLATFORM}_${ARCHITECTURE}

rm -rf ../include $BINARY
mkdir -p ../include $BINARY

cp -r JavaScriptCore/Headers/* ../include/
cp lib/*.a $BINARY/

ls ../include/JavaScriptCore/*.h | xargs -n1 sed -E -e 's|<JavaScriptCore/([A-Za-z0-9_\.]+)>|"\1"|' -i
