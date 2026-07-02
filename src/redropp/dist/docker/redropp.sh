#!/bin/sh

set -e

TARGET=redropp
IMAGE_ARCHITECTURES="amd64 arm64"

DOCKER_PATH=src/redropp/dist/docker/Dockerfile

cd "$(dirname $0)/../../../.."
. tools/package/build/docker/package.sh
