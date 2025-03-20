#!/bin/bash -e

dpkg --add-architecture i386
dpkg --add-architecture arm64

apt update
apt install -y build-essential curl git cmake ninja-build pkg-config gdb debhelper dh-make devscripts \
               gcc-i686-linux-gnu g++-i686-linux-gnu binutils-i686-linux-gnu libc6:i386 \
               gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu libc6:arm64 \
               cmake ninja-build perl ruby nodejs npm

curl https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-13 main" >> /etc/apt/sources.list
echo "deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-13 main" >> /etc/apt/sources.list
echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-18 main" >> /etc/apt/sources.list
echo "deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-18 main" >> /etc/apt/sources.list
echo "deb http://deb.debian.org/debian bullseye-backports main" >> /etc/apt/sources.list

apt update
apt install -y clang-13 lld-13 clang-18 lld-18
