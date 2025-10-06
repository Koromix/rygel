#!/bin/sh

set -e

TARGET=goupile
IMAGE_ARCHITECTURES="amd64 arm64"

DOCKER_PATH=src/goupile/dist/docker/Dockerfile

cd "$(dirname $0)/../../../.."
. tools/package/build/docker/package.sh
