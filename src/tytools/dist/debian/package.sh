#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug TyCommander

VERSION=$(bin/Debug/TyCommander --version | awk -F'[ _]' '/^TyCommander/ {print $2}')
DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/tytools/debian

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg $PACKAGE_DIR/pkg/debian

docker build -t rygel/ubuntu22.04 deploy/docker/ubuntu22.04
docker run -t -i --rm -v $(pwd):/io rygel/ubuntu22.04 /io/src/tytools/dist/debian/build.sh

install -D -m0755 bin/Packages/tytools/debian/bin/tycmd $PACKAGE_DIR/pkg/tycmd
install -D -m0755 bin/Packages/tytools/debian/bin/TyCommander $PACKAGE_DIR/pkg/TyCommander
install -D -m0755 bin/Packages/tytools/debian/bin/TyUploader $PACKAGE_DIR/pkg/TyUploader
install -D -m0644 src/tytools/tycommander/tycommander_linux.desktop $PACKAGE_DIR/pkg/TyCommander.desktop
install -D -m0644 src/tytools/tyuploader/tyuploader_linux.desktop $PACKAGE_DIR/pkg/TyUploader.desktop
install -D -m0644 src/tytools/assets/images/tycommander.png $PACKAGE_DIR/pkg/TyCommander.png
install -D -m0644 src/tytools/assets/images/tyuploader.png $PACKAGE_DIR/pkg/TyUploader.png
install -D -m0644 src/tytools/dist/debian/teensy.rules $PACKAGE_DIR/pkg/00-teensy.rules

install -D -m0755 src/tytools/dist/debian/rules $PACKAGE_DIR/pkg/debian/rules
install -D -m0644 src/tytools/dist/debian/compat $PACKAGE_DIR/pkg/debian/compat
install -D -m0644 src/tytools/dist/debian/install $PACKAGE_DIR/pkg/debian/install
install -D -m0644 src/tytools/dist/debian/format $PACKAGE_DIR/pkg/debian/source/format

echo "\
Source: tytools
Section: utils
Priority: optional
Maintainer: Niels Martignène <niels.martignene@protonmail.com>
Standards-Version: 4.5.1
Rules-Requires-Root: no

Package: tytools
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: GUI and command-line tools to manage Teensy devices
" > $PACKAGE_DIR/pkg/debian/control
echo "\
tytools ($VERSION) unstable; urgency=low

  * Current release.

 -- Niels Martignène <niels.martignene@protonmail.com>  $DATE
" > $PACKAGE_DIR/pkg/debian/changelog

(cd $PACKAGE_DIR/pkg && dpkg-buildpackage -uc -us)
cp $PACKAGE_DIR/*.deb $PACKAGE_DIR/../
