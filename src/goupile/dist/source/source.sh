#!/bin/sh

set -e

PKG_NAME=goupile
BUILD_TARGETS=goupile
VERSION_TARGET=goupile

adjust() {
    cp src/goupile/*.md "$1"
}

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh

