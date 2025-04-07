#!/bin/sh

set -e
cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pParanoid rekkord

VERSION=$(bin/Fast/rekkord --version | awk -F'[ _]' '/^rekkord/ {print $2}')
PACKAGE_DIR=bin/Packages/rekkord/macos
DMG_FILENAME=bin/Packages/rekkord_${VERSION}_macos.dmg

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg

install -m0755 bin/Fast/rekkord $PACKAGE_DIR/pkg/rekkord
install -m0644 src/rekkord/README.md $PACKAGE_DIR/pkg/README.md
install -m0644 LICENSE.txt $PACKAGE_DIR/pkg/LICENSE.txt

rm -f $DMG_FILENAME
hdiutil create -srcfolder "$PACKAGE_DIR/pkg" -volname "rekkord" $DMG_FILENAME
