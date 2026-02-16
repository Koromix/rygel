#!/bin/sh -e

cd "$(dirname $0)"

rm -rf freebsd_amd64
mkdir freebsd_amd64

if tar --version | grep -q gnu; then
    TAR='tar --warning=no-unknown-keyword'
else
    TAR=tar
fi

curl -L -o- https://download.freebsd.org/ftp/releases/amd64/14.3-RELEASE/base.txz | $TAR -xJ -C freebsd_amd64  ./lib/ ./usr/lib/ ./usr/include
