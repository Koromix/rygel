#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../sysroots/freebsd_arm64
mkdir ../sysroots/freebsd_arm64

if tar --version | grep -q gnu; then
    TAR='tar --warning=no-unknown-keyword'
else
    TAR=tar
fi

curl -L -o- https://download.freebsd.org/ftp/releases/arm64/14.3-RELEASE/base.txz | $TAR -xJ -C ../sysroots/freebsd_arm64  ./lib/ ./usr/lib/ ./usr/include
