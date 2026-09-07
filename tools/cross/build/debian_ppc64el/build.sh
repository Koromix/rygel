#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../../sysroots/debian_ppc64el
mkdir -p ../../machines/debian_ppc64el ../../sysroots/debian_ppc64el

podman build -t rygel/debian13 ../../../docker/debian13

podman run --privileged --rm \
    -v $PWD:/host:ro \
    -v $PWD/../../machines/debian_ppc64el:/dest \
    -v $PWD/../../sysroots/debian_ppc64el:/sysroot \
    rygel/debian13 /host/stage1.sh
