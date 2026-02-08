#!/bin/sh -e

cd "$(dirname $0)"

mkdir -p ../../machines/debian_loong64

podman build -t build/debian_loong64 .
podman run --privileged --rm -v $PWD/../../machines/debian_loong64:/dest build/debian_loong64 /stage2.sh

cp QEMU_EFI.fd $PWD/../../machines/debian_loong64/QEMU_EFI.fd

cd ../..

tar -cSv machines/debian_loong64/* | zstd --fast > qemu_debian_loong64.tar.zst

old=$(grep -v debian_loong64 b3sum.txt)
echo "$old" > b3sum.txt
b3sum machines/debian_loong64/* >> b3sum.txt
