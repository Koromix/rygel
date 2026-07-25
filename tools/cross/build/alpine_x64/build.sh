#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/alpine_x64
mkdir -p ../../machines/alpine_x64 ../../sysroots/alpine_x64

podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../sysroots/alpine_x64:/sysroot \
    alpine:3.22 /host/stage1.sh
