#!/bin/sh

set -e

PKG_NAME=rekkord
BUILD_TARGETS="rekkord RekkordTray"
VERSION_TARGET=rekkord
FELIX_PRESET=Fast

adjust() {
    cp src/rekkord/*.md "$1"

    rm -rf "$1/vendor/curl/docs" "$1/vendor/curl/tests"
    rm -rf "$1/vendor/mbedtls/docs" "$1/vendor/mbedtls/tests"
}

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh
