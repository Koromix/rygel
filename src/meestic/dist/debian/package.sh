#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug meestic

VERSION=$(bin/Debug/meestic --version | awk -F'[ _]' '/^meestic/ {print $2}')
DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/meestic/debian

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg $PACKAGE_DIR/pkg/debian

docker build -t rygel/debian10 deploy/docker/debian10
docker run -t -i --rm -v $(pwd):/io rygel/debian10 /io/src/meestic/dist/debian/build.sh

install -D -m0755 bin/Packages/meestic/debian/bin/meestic $PACKAGE_DIR/pkg/meestic
install -D -m0755 bin/Packages/meestic/debian/bin/MeesticTray $PACKAGE_DIR/pkg/MeesticTray
install -D -m0644 src/meestic/MeesticTray.ini $PACKAGE_DIR/pkg/meestic.ini
install -D -m0644 src/meestic/dist/debian/MeesticTray.desktop $PACKAGE_DIR/pkg/MeesticTray.desktop
install -D -m0644 src/meestic/images/meestic.png $PACKAGE_DIR/pkg/MeesticTray.png

install -D -m0644 src/meestic/dist/debian/meestic.service $PACKAGE_DIR/pkg/debian/meestic.service
install -D -m0755 src/meestic/dist/debian/rules $PACKAGE_DIR/pkg/debian/rules
install -D -m0644 src/meestic/dist/debian/compat $PACKAGE_DIR/pkg/debian/compat
install -D -m0644 src/meestic/dist/debian/install $PACKAGE_DIR/pkg/debian/install
install -D -m0644 src/meestic/dist/debian/format $PACKAGE_DIR/pkg/debian/source/format

echo "\
Source: meestic
Section: utils
Priority: optional
Maintainer: Niels Martignène <niels.martignene@protonmail.com>
Standards-Version: 4.5.1
Rules-Requires-Root: no

Package: meestic
Architecture: any
Depends: libudev1
Description: CLI and GUI tools to control the keyboard lighting on MSI Delta 15 laptops
" > $PACKAGE_DIR/pkg/debian/control
echo "\
meestic ($VERSION) unstable; urgency=low

  * Current release.

 -- Niels Martignène <niels.martignene@protonmail.com>  $DATE
" > $PACKAGE_DIR/pkg/debian/changelog

(cd $PACKAGE_DIR/pkg && dpkg-buildpackage -uc -us)
cp $PACKAGE_DIR/*.deb $PACKAGE_DIR/../

./bootstrap.sh
