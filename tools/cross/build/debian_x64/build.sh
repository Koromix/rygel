#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_x64
mkdir -p ../../machines/debian_x64 ../../sysroots/debian_x64

podman build -t rygel/debian13 ../../../docker/debian13

podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../machines/debian_x64:/dest \
    -v $PWD/../../sysroots/debian_x64:/sysroot \
    rygel/debian13 /host/stage1.sh

cd ../..

tar -cSv machines/debian_x64/* | zstd --fast > qemu_debian_x64.tar.zst

old=$(grep -v debian_x64 machines.b3sum)
echo "$old" > machines.b3sum
b3sum machines/debian_x64/* >> machines.b3sum
