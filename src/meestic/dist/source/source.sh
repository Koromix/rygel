#!/bin/sh

set -e

PKG_NAME=meestic
BUILD_TARGETS="meestic MeesticTray"
VERSION_TARGET=meestic
FELIX_PRESET=Fast

adjust() {
    cp src/meestic/*.md "$1"

    rm -rf "$1/vendor/curl/docs" "$1/vendor/curl/tests"
    rm -rf "$1/vendor/mbedtls/docs" "$1/vendor/mbedtls/tests"
}

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh
