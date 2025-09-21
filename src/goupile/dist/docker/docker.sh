#!/bin/sh

set -e

TARGET=goupile
DOCKER_PATH=src/goupile/dist/docker/Dockerfile

cd "$(dirname $0)/../../../.."
. tools/package/build/docker/package.sh
