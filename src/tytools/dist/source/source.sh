#!/bin/sh

set -e

PKG_NAME=tytools
BUILD_TARGETS="tycmd tycommander tyuploader"
VERSION_TARGET=tycmd

adjust() {
    cp src/tytools/*.md src/tytools/LICENSE* "$1"

    rm -rf "$1/vendor/curl/docs" "$1/vendor/curl/tests"
    rm -rf "$1/vendor/mbedtls/docs" "$1/vendor/mbedtls/tests"
}

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh
