#!/bin/sh

set -e

TARGET=RekkordDrop
IMAGE_NAME=koromix/rekkorddrop
IMAGE_ARCHITECTURES="amd64 arm64"

DOCKER_PATH=src/rekkord/dist/docker/RekkordDrop/Dockerfile

cd "$(dirname $0)/../../../../.."
. tools/package/build/docker/package.sh
