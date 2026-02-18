#!/bin/sh -e

cd "$(dirname $0)"

SYSROOT=../../sysroots/freebsd_x64

rm -rf $SYSROOT
mkdir $SYSROOT

if tar --version | grep -q gnu; then
    TAR='tar --warning=no-unknown-keyword'
else
    TAR=tar
fi

curl -L -o- https://download.freebsd.org/ftp/releases/amd64/14.3-RELEASE/base.txz | $TAR -xJ -C $SYSROOT  ./lib/ ./usr/lib/ ./usr/include
