#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_i386
mkdir -p ../../machines/debian_i386 ../../sysroots/debian_i386

podman build -t build/debian_i386 .
podman run --privileged --rm \
    -v $PWD/../../machines/debian_i386:/dest \
    build/debian_i386 /stage2.sh
podman run --privileged --rm \
    -v $PWD/../../sysroots/debian_i386:/sysroot \
    build/debian_i386 /stage3.sh

cd ../..

tar -cSv machines/debian_i386/* | zstd --fast > qemu_debian_i386.tar.zst

old=$(grep -v debian_i386 machines.b3sum)
echo "$old" > machines.b3sum
b3sum machines/debian_i386/* >> machines.b3sum
