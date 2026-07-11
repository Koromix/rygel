#!/bin/sh

set -e

PKG_NAME=redropp
BUILD_TARGETS="redropp"
VERSION_TARGET=redropp
FELIX_PRESET=Paranoid

adjust() {
    cp src/redropp/*.md "$1"

    rm -rf "$1/vendor/curl/docs" "$1/vendor/curl/tests"
    rm -rf "$1/vendor/mbedtls/docs" "$1/vendor/mbedtls/tests"
}

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh
