#!/bin/sh

set -e
cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pFast rekord

VERSION=$(bin/Fast/rekord --version | awk -F'[ _]' '/^rekord/ {print $2}')
PACKAGE_DIR=bin/Packages/rekord/macos
DMG_FILENAME=bin/Packages/rekord_${VERSION}_macos.dmg

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg

install -m0755 bin/Fast/rekord $PACKAGE_DIR/pkg/rekord
install -m0644 src/rekord/README.md $PACKAGE_DIR/pkg/README.md
install -m0644 LICENSE.txt $PACKAGE_DIR/pkg/LICENSE.txt

rm -f $DMG_FILENAME
hdiutil create -srcfolder "$PACKAGE_DIR/pkg" -volname "Rekord" $DMG_FILENAME
