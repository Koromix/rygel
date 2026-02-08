#!/bin/sh -e

cd "$(dirname $0)"

mkdir -p ../../machines/debian_arm64

podman build -t build/debian_arm64 .
podman run --privileged --rm -v $PWD/../../machines/debian_arm64:/dest build/debian_arm64 /stage2.sh

cd ../..

tar -cSv machines/debian_arm64/* | zstd --fast > qemu_debian_arm64.tar.zst

old=$(grep -v debian_arm64 b3sum.txt)
echo "$old" > b3sum.txt
b3sum machines/debian_arm64/* >> b3sum.txt
