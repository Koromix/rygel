#!/bin/sh

set -e

PKG_NAME=rekord
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Backup tool with deduplication and asymmetric encryption"
PKG_DEPENDENCIES=""
PKG_LICENSE=AGPL-3.0-or-later
PKG_ARCHITECTURES="amd64 i386 arm64"

SCRIPT_PATH=src/rekord/dist/linux/debian.sh
VERSION_TARGET=rekord
DOCKER_IMAGE=debian11

build() {
    ./bootstrap.sh
    ./felix -pFast --host=$1 rekord

    install -D -m0755 bin/Fast/rekord ${ROOT_DIR}/usr/bin/rekord
}

cd "$(dirname $0)/../../../.."
. deploy/package/debian/package.sh
