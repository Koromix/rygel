#!/bin/sh -e

cd "$(dirname $0)"

rm -rf freebsd_amd64
mkdir freebsd_amd64

curl -L -o- https://download.freebsd.org/ftp/releases/amd64/14.3-RELEASE/base.txz | tar -xJ -C freebsd_amd64  ./lib/ ./usr/lib/ ./usr/include
