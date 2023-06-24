#!/bin/sh -e

cd "$(dirname $0)/../../../.."

docker build -t rygel/debian10 deploy/docker/debian10
docker run -t -i --rm -v $(pwd):/io rygel/debian10 /io/src/meestic/dist/debian/build.sh

VERSION=$(./felix --version | awk '/^felix/ {print $2}' | sed -e 's/_.*//')
DATE=$(echo $VERSION | sed 's/\./ /' | LANG=C xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/meestic_${VERSION}_debian

rm -rf $PACKAGE_DIR
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/src $PACKAGE_DIR/src/debian

install -D -m0755 bin/Fast/meestic $PACKAGE_DIR/src/meestic
install -D -m0755 bin/Fast/MeesticGui $PACKAGE_DIR/src/MeesticGui
install -D -m0644 src/meestic/dist/debian/meestic.ini $PACKAGE_DIR/src/meestic.ini
install -D -m0644 src/meestic/dist/debian/MeesticGui.desktop $PACKAGE_DIR/src/MeesticGui.desktop
install -D -m0644 src/meestic/images/meestic.png $PACKAGE_DIR/src/MeesticGui.png

install -D -m0644 src/meestic/dist/debian/meestic.service $PACKAGE_DIR/src/debian/meestic.service
install -D -m0755 src/meestic/dist/debian/rules $PACKAGE_DIR/src/debian/rules
install -D -m0644 src/meestic/dist/debian/compat $PACKAGE_DIR/src/debian/compat
install -D -m0644 src/meestic/dist/debian/install $PACKAGE_DIR/src/debian/install
install -D -m0644 src/meestic/dist/debian/format $PACKAGE_DIR/src/debian/source/format

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
" > ${PACKAGE_DIR}/src/debian/control
echo "\
meestic ($VERSION) unstable; urgency=low

  * Current release.

 -- Niels Martignène <niels.martignene@protonmail.com>  $DATE
" > ${PACKAGE_DIR}/src/debian/changelog

cd ${PACKAGE_DIR}/src
dpkg-buildpackage -uc -us

# cd ${PACKAGE_DIR}/..
# dpkg-deb --root-owner-group --build $(basename $PACKAGE_DIR)
