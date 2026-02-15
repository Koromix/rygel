#!/bin/sh -e

cd "$(dirname $0)"

rm -rf freebsd_arm64
mkdir freebsd_arm64

curl -L -o- https://download.freebsd.org/ftp/releases/arm64/14.3-RELEASE/base.txz | tar -xJ -C freebsd_arm64  ./lib/ ./usr/lib/ ./usr/include
