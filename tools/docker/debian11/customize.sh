#!/bin/bash -e

dpkg --add-architecture arm64

apt update
apt install -y build-essential curl git cmake ninja-build pkg-config gdb debhelper dh-make devscripts \
               gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu libc6:arm64 \
               cmake ninja-build perl ruby nodejs npm patchelf \
               fakeroot debootstrap libguestfs-tools symlinks qemu-user-static rsync

curl https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-18 main" >> /etc/apt/sources.list
echo "deb-src http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-18 main" >> /etc/apt/sources.list
echo "deb http://deb.debian.org/debian bullseye-backports main" >> /etc/apt/sources.list

apt update
apt install -y clang-18 lld-18 libclang-rt-18-dev:arm64
