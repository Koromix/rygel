#!/bin/bash -e

curl https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-15 main" >> /etc/apt/sources.list
echo "deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-15 main" >> /etc/apt/sources.list

apt update
apt install -y build-essential git cmake ninja-build pkg-config gdb debhelper dh-make devscripts clang-15 lld-15
