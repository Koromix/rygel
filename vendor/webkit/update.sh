#!/bin/sh -e

ICU=release-77-1
WEBKIT=WebKit-7620.2.4.111.7
DOCKER_IMAGE=debian11

set -e
cd $(dirname $0)

PLATFORM=$(../../felix --version | awk -F '[:/ ]' '/^Host:/ { print $NF }')
ARCHITECTURE=$(../../felix --version | awk -F '[:/ ]' '/^Architecture:/ { print $NF }')

docker build -t rygel/${DOCKER_IMAGE} ../../tools/docker/${DOCKER_IMAGE}

docker run --privileged -t -i --rm \
    -v $(pwd)/build:/io/host -v $(pwd)/../_patches:/io/host/patches \
    -e ICU=${ICU} -e WEBKIT=${WEBKIT} \
    -e PLATFORM=${PLATFORM} -e ARCHITECTURE=${ARCHITECTURE} \
    rygel/${DOCKER_IMAGE} /io/host/build.js

rsync -rtvp --delete build/include/ include/
rsync -rtvp --delete build/lib/${PLATFORM}/${ARCHITECTURE}/ lib/${PLATFORM}/${ARCHITECTURE}/
