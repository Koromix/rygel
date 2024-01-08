#!/bin/bash -e

dpkg --add-architecture i386
dpkg --add-architecture arm64

apt update
apt install -y build-essential curl git cmake ninja-build pkg-config gdb debhelper dh-make devscripts clang lld \
               gcc-i686-linux-gnu g++-i686-linux-gnu binutils-i686-linux-gnu libc6:i386 \
               gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu libc6:arm64
