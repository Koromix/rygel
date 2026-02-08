#!/bin/sh -e

cd "$(dirname $0)"

mkdir -p ../../machines/debian_i386

podman build -t build/debian_i386 .
podman run --privileged --rm -v $PWD/../../machines/debian_i386:/dest build/debian_i386 /stage2.sh

cd ../..

tar -cSv machines/debian_i386/* | zstd --fast > qemu_debian_i386.tar.zst

old=$(grep -v debian_i386 b3sum.txt)
echo "$old" > b3sum.txt
b3sum machines/debian_i386/* >> b3sum.txt
