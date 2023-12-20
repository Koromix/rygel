#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug goupile

VERSION=$(bin/Debug/goupile --version | awk -F'[ _]' '/^goupile/ {print $2}')
DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/goupile/debian

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg $PACKAGE_DIR/pkg/debian

docker build -t rygel/debian11 deploy/docker/debian11
docker run -t -i --rm -v $(pwd):/io rygel/debian11 /io/src/goupile/dist/debian/build.sh

install -D -m0755 bin/Packages/goupile/debian/bin/goupile $PACKAGE_DIR/pkg/goupile

install -D -m0644 src/goupile/dist/debian/goupile.service $PACKAGE_DIR/pkg/debian/goupile.service
install -D -m0755 src/goupile/dist/debian/rules $PACKAGE_DIR/pkg/debian/rules
install -D -m0644 src/goupile/dist/debian/compat $PACKAGE_DIR/pkg/debian/compat
install -D -m0644 src/goupile/dist/debian/install $PACKAGE_DIR/pkg/debian/install
install -D -m0644 src/goupile/dist/debian/postinst $PACKAGE_DIR/pkg/debian/postinst
install -D -m0644 src/goupile/dist/debian/format $PACKAGE_DIR/pkg/debian/source/format

echo "\
Source: goupile
Section: utils
Priority: optional
Maintainer: Niels Martignène <niels.martignene@protonmail.com>
Standards-Version: 4.5.1
Rules-Requires-Root: no

Package: goupile
Architecture: any
Depends: libudev1
Description: CLI and GUI tools to control the keyboard lighting on MSI Delta 15 laptops
" > $PACKAGE_DIR/pkg/debian/control
echo "\
goupile ($VERSION) unstable; urgency=low

  * Current release.

 -- Niels Martignène <niels.martignene@protonmail.com>  $DATE
" > $PACKAGE_DIR/pkg/debian/changelog

(cd $PACKAGE_DIR/pkg && dpkg-buildpackage -uc -us)
cp $PACKAGE_DIR/*.deb $PACKAGE_DIR/../

./bootstrap.sh
