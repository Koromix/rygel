#!/bin/sh

set -e

PKG_NAME=rekkord
PKG_AUTHOR="Niels Martignène <niels.martignene@protonmail.com>"
PKG_DESCRIPTION="Backup tool with deduplication and asymmetric encryption"
PKG_DEPENDENCIES=""
PKG_LICENSE=GPL-3.0-or-later

SCRIPT_PATH=src/rekkord/dist/linux/rpm.sh
VERSION_TARGET=rekkord
DOCKER_IMAGE=rocky9

build() {
    ./bootstrap.sh
    ./felix -pFast rekkord

    install -D -m0755 bin/Fast/rekkord ${ROOT_DIR}/usr/bin/rekkord
}

cd "$(dirname $0)/../../../.."
. tools/package/build/rpm/package.sh
