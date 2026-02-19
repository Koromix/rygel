#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_arm64
mkdir -p ../../machines/debian_arm64 ../../sysroots/debian_arm64

podman build -t rygel/debian13 ../../../docker/debian13

podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../machines/debian_arm64:/dest \
    -v $PWD/../../sysroots/debian_arm64:/sysroot \
    rygel/debian13 /host/stage1.sh

cd ../..

tar -cSv machines/debian_arm64/* | zstd --fast > qemu_debian_arm64.tar.zst

old=$(grep -v debian_arm64 machines.b3sum)
echo "$old" > machines.b3sum
b3sum machines/debian_arm64/* >> machines.b3sum
