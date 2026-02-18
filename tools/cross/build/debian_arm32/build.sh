#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_arm32
mkdir -p ../../machines/debian_arm32 ../../sysroots/debian_arm32

podman build -t build/debian_arm32 .
podman run --privileged --rm \
    -v $PWD/../../machines/debian_arm32:/dest \
    -v $PWD/../../sysroots/debian_arm32:/sysroot \
    build/debian_arm32 /stage2.sh

cd ../..

tar -cSv machines/debian_arm32/* | zstd --fast > qemu_debian_arm32.tar.zst

old=$(grep -v debian_arm32 machines.b3sum)
echo "$old" > machines.b3sum
b3sum machines/debian_arm32/* >> machines.b3sum
