#!/bin/sh

set -e

PKG_NAME=felix
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Small build system made specifically for this repository"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later
PKG_ARCHITECTURES="amd64 arm64"

SCRIPT_PATH=src/felix/dist/linux/debian.sh
VERSION_TARGET=felix
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh
    ./felix -pFast --host=$1:clang-18:lld-18 felix

    install -D -m0755 bin/Fast/felix ${ROOT_DIR}/usr/bin/felix
}

cd "$(dirname $0)/../../../.."
. tools/package/build/debian/package.sh
