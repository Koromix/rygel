#!/bin/sh

set -e

PKG_NAME=goupile
BUILD_TARGETS=goupile
VERSION_TARGET=goupile

adjust() {
    cp src/goupile/*.md "$1"

    rm -rf "$1/vendor/ace/src" "$1/vendor/ace/src-noconflict" "$1/vendor/ace/src-min-noconflict"
    rm -rf "$1/vendor/curls/docs" "$1/vendor/curls/tests"
    rm -rf "$1/vendor/mbedtls/docs" "$1/vendor/mbedtls/tests"

    rm -rf "$1/vendor/esbuild/native/node_modules/@esbuild/darwin-arm64" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/darwin-x64" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/freebsd-arm64" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/freebsd-x64" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/linux-ia32" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/openbsd-arm64" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/openbsd-x64" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/win32-arm64" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/win32-ia32" \
           "$1/vendor/esbuild/native/node_modules/@esbuild/win32-x64"
}

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh

