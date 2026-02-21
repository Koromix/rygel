#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_loong64
mkdir -p ../../machines/debian_loong64 ../../sysroots/debian_loong64

podman build -t rygel/debian13 ../../../docker/debian13

podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../machines/debian_loong64:/dest \
    -v $PWD/../../sysroots/debian_loong64:/sysroot \
    rygel/debian13 /host/stage1.sh

cp QEMU_EFI.fd $PWD/../../machines/debian_loong64/QEMU_EFI.fd
