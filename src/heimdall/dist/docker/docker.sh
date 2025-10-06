#!/bin/sh

set -e

TARGET=heimdall
IMAGE_ARCHITECTURES="amd64 arm64"

DOCKER_PATH=src/heimdall/dist/docker/Dockerfile

cd "$(dirname $0)/../../../.."
. tools/package/build/docker/package.sh
