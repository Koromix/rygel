#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_riscv64
mkdir -p ../../machines/debian_riscv64 ../../sysroots/debian_riscv64

podman build -t rygel/debian13 ../../../docker/debian13

podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../machines/debian_riscv64:/dest \
    -v $PWD/../../sysroots/debian_riscv64:/sysroot \
    rygel/debian13 /host/stage1.sh
