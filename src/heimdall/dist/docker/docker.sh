#!/bin/sh

set -e

TARGET=heimdall
DOCKER_PATH=src/heimdall/dist/docker/Dockerfile

cd "$(dirname $0)/../../../.."
. tools/package/build/docker/package.sh
