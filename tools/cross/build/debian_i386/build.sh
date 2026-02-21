#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_i386
mkdir -p ../../machines/debian_i386 ../../sysroots/debian_i386

podman build -t rygel/debian13 ../../../docker/debian13

podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../sysroots/debian_i386:/sysroot \
    rygel/debian13 /host/stage1.sh
podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../machines/debian_i386:/dest \
    rygel/debian13 /host/stage2.sh
