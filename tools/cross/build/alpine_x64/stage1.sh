#!/bin/sh -e

cd /

apk update
apk add --no-cache alpine-sdk

cp -r /lib /sysroot/lib

mkdir /sysroot/usr
cp -r /usr/include /sysroot/usr/include
cp -r /usr/lib /sysroot/usr/lib
cp -r /usr/bin /sysroot/usr/bin

ln -s x86_64-alpine-linux-musl /sysroot/usr/lib/gcc/x86_64-linux-musl
ln -s x86_64-alpine-linux-musl /sysroot/usr/include/c++/14.2.0/x86_64-linux-musl
