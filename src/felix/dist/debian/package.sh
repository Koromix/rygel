#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug felix

VERSION=$(bin/Debug/felix --version | awk -F'[ _]' '/^felix/ {print $2}')
DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/felix/debian

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg $PACKAGE_DIR/pkg/debian

docker build -t rygel/debian10 deploy/docker/debian10
docker run -t -i --rm -v $(pwd):/io rygel/debian10 /io/src/felix/dist/debian/build.sh

install -D -m0755 bin/Packages/felix/debian/bin/felix $PACKAGE_DIR/pkg/felix

install -D -m0755 src/felix/dist/debian/rules $PACKAGE_DIR/pkg/debian/rules
install -D -m0644 src/felix/dist/debian/compat $PACKAGE_DIR/pkg/debian/compat
install -D -m0644 src/felix/dist/debian/install $PACKAGE_DIR/pkg/debian/install
install -D -m0644 src/felix/dist/debian/format $PACKAGE_DIR/pkg/debian/source/format

echo "\
Source: felix
Section: utils
Priority: optional
Maintainer: Niels Martignène <niels.martignene@protonmail.com>
Standards-Version: 4.5.1
Rules-Requires-Root: no

Package: felix
Architecture: any
Depends: libudev1
Description: CLI and GUI tools to control the keyboard lighting on MSI Delta 15 laptops
" > $PACKAGE_DIR/pkg/debian/control
echo "\
felix ($VERSION) unstable; urgency=low

  * Current release.

 -- Niels Martignène <niels.martignene@protonmail.com>  $DATE
" > $PACKAGE_DIR/pkg/debian/changelog

(cd $PACKAGE_DIR/pkg && dpkg-buildpackage -uc -us)
cp $PACKAGE_DIR/*.deb $PACKAGE_DIR/../

./bootstrap.sh
