#!/bin/sh -e

PKG_NAME=rekord
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Backup tool with deduplication and asymmetric encryption"
PKG_DEPENDENCIES=""

SCRIPT_PATH=src/rekord/dist/debian/package.sh
VERSION_TARGET=rekord
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh --no_user
    ./felix -pFast --no_user -O "${BUILD_DIR}" rekord
}

package() {
    install -D -m0755 ${BUILD_DIR}/rekord ${ROOT_DIR}/usr/bin/rekord
}

cd "$(dirname $0)/../../../.."
. deploy/debian/package/package.sh
