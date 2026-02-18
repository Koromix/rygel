#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_riscv64
mkdir -p ../../machines/debian_riscv64 ../../sysroots/debian_riscv64

podman build -t build/debian_riscv64 .
podman run --privileged --rm \
    -v $PWD/../../machines/debian_riscv64:/dest \
    -v $PWD/../../sysroots/debian_riscv64:/sysroot \
    build/debian_riscv64 /stage2.sh

cd ../..

tar -cSv machines/debian_riscv64/* | zstd --fast > qemu_debian_riscv64.tar.zst

old=$(grep -v debian_riscv64 machines.b3sum)
echo "$old" > machines.b3sum
b3sum machines/debian_riscv64/* >> machines.b3sum
