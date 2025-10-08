#!/bin/sh

set -e

TARGET=rokkerd
IMAGE_ARCHITECTURES="amd64 arm64"

DOCKER_PATH=src/rokkerd/dist/docker/Dockerfile

cd "$(dirname $0)/../../../.."
. tools/package/build/docker/package.sh
