#!/bin/bash -e

apt update
apt install -y build-essential git cmake ninja-build pkg-config gdb debhelper dh-make devscripts \
               gcc-i686-linux-gnu g++-i686-linux-gnu binutils-i686-linux-gnu \
               gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu \
               gcc-riscv64-linux-gnu g++-riscv64-linux-gnu binutils-riscv64-linux-gnu

curl https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-15 main" >> /etc/apt/sources.list
echo "deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-15 main" >> /etc/apt/sources.list

apt update
apt install -y clang-15 lld-15
