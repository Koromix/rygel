#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug rekord

VERSION=$(bin/Debug/rekord --version | awk -F'[ _]' '/^rekord/ {print $2}')
DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/rekord/debian

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg $PACKAGE_DIR/pkg/debian

docker build -t rygel/debian10 deploy/docker/debian10
docker run -t -i --rm -v $(pwd):/io rygel/debian10 /io/src/rekord/dist/debian/build.sh

install -D -m0755 bin/Packages/rekord/debian/bin/rekord $PACKAGE_DIR/pkg/rekord

install -D -m0755 src/rekord/dist/debian/rules $PACKAGE_DIR/pkg/debian/rules
install -D -m0644 src/rekord/dist/debian/compat $PACKAGE_DIR/pkg/debian/compat
install -D -m0644 src/rekord/dist/debian/install $PACKAGE_DIR/pkg/debian/install
install -D -m0644 src/rekord/dist/debian/format $PACKAGE_DIR/pkg/debian/source/format

echo "\
Source: rekord
Section: utils
Priority: optional
Maintainer: Niels Martignène <niels.martignene@protonmail.com>
Standards-Version: 4.5.1
Rules-Requires-Root: no

Package: rekord
Architecture: any
Description: Backup tool with deduplication and asymmetric encryption
" > $PACKAGE_DIR/pkg/debian/control
echo "\
rekord ($VERSION) unstable; urgency=low

  * Current release.

 -- Niels Martignène <niels.martignene@protonmail.com>  $DATE
" > $PACKAGE_DIR/pkg/debian/changelog

(cd $PACKAGE_DIR/pkg && dpkg-buildpackage -uc -us)
cp $PACKAGE_DIR/*.deb $PACKAGE_DIR/../

./bootstrap.sh
