#!/bin/sh

set -e

PKG_NAME=felix
BUILD_TARGETS="felix"
VERSION_TARGET=felix
FELIX_PRESET=Fast

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh
