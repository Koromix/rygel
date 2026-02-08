#!/bin/sh -e

cd "$(dirname $0)"

mkdir -p ../../machines/debian_arm32

podman build -t build/debian_arm32 .
podman run --privileged --rm -v $PWD/../../machines/debian_arm32:/dest build/debian_arm32 /stage2.sh

cd ../..

tar -cSv machines/debian_arm32/* | zstd --fast > qemu_debian_arm32.tar.zst

old=$(grep -v debian_arm32 b3sum.txt)
echo "$old" > b3sum.txt
b3sum machines/debian_arm32/* >> b3sum.txt
