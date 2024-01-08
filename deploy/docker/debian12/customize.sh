#!/bin/bash -e

apt update
apt install -y build-essential git cmake ninja-build pkg-config gdb debhelper dh-make devscripts clang lld \
               gcc-i686-linux-gnu g++-i686-linux-gnu binutils-i686-linux-gnu \
               gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu \
               gcc-riscv64-linux-gnu g++-riscv64-linux-gnu binutils-riscv64-linux-gnu
