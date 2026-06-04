#!/bin/sh

set -e

PKG_NAME=heimdall
BUILD_TARGETS=heimdall
VERSION_TARGET=heimdall
FELIX_PRESET=Paranoid

cd "$(dirname $0)/../../../.."
. tools/package/build/source/package.sh
