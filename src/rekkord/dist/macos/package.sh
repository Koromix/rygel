#!/bin/sh

set -e
cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug rekkord

VERSION=$(bin/Debug/rekkord --version | awk -F'[ _]' '/^rekkord/ {print $2}')
PACKAGE_DIR=bin/Packages/rekkord/macos
DMG_FILENAME=bin/Packages/rekkord_${VERSION}_macos.dmg

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg

install -m0644 src/rekkord/README.md $PACKAGE_DIR/pkg/README.md
install -m0644 LICENSE.txt $PACKAGE_DIR/pkg/LICENSE.txt

./felix -pFast --host=x86_64 -O bin/x86_64 rekkord
./felix -pFast --host=ARM64 -O bin/ARM64 rekkord
lipo -create -output $PACKAGE_DIR/pkg/rekkord bin/x86_64/rekkord bin/ARM64/rekkord

rm -f "$DMG_FILENAME"
hdiutil create -srcfolder "$PACKAGE_DIR/pkg" -volname "rekkord" "$DMG_FILENAME"
