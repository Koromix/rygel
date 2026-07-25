#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/alpine_arm64
mkdir -p ../../machines/alpine_arm64 ../../sysroots/alpine_arm64

podman run --platform linux/arm64 --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../sysroots/alpine_arm64:/sysroot \
    alpine:3.22 /bin/sh /host/stage1.sh
