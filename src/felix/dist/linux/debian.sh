#!/bin/sh -e

PKG_NAME=felix
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Small build system made specifically for this repository"
PKG_DEPENDENCIES=""
PKG_LICENSE=AGPL-3.0-or-later

SCRIPT_PATH=src/felix/dist/linux/debian.sh
VERSION_TARGET=felix
DOCKER_IMAGE=debian10

build() {
    ./bootstrap.sh
    ./felix -pFast felix

    install -D -m0755 bin/Fast/felix ${ROOT_DIR}/usr/bin/felix
}

cd "$(dirname $0)/../../../.."
. deploy/debian/package/package.sh
