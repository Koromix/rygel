#!/bin/sh

set -e
cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug rekkord

VERSION=$(bin/Debug/rekkord --version | awk -F'[ _]' '/^rekkord/ {print $2}')
PACKAGE_DIR=bin/Packages/rekkord/macos/rekkord_${VERSION}
DMG_FILENAME=bin/Packages/rekkord_${VERSION}_macos.dmg
TAR_FILENAME=bin/Packages/rekkord_${VERSION}_macos.tar

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

install -m0644 src/rekkord/README.md "$PACKAGE_DIR/README.md"
install -m0644 LICENSE.txt "$PACKAGE_DIR/LICENSE.txt"

./felix -pFast --host=x86_64 -O bin/x86_64 rekkord
./felix -pFast --host=ARM64 -O bin/ARM64 rekkord
lipo -create -output "$PACKAGE_DIR/rekkord" bin/x86_64/rekkord bin/ARM64/rekkord

rm -f "$DMG_FILENAME"
hdiutil create -srcfolder "$PACKAGE_DIR" -volname "rekkord" "$DMG_FILENAME"
tar -C "$(dirname $PACKAGE_DIR)" -c "$(basename $PACKAGE_DIR)" | gzip > "$TAR_FILENAME.gz"
tar -C "$(dirname $PACKAGE_DIR)" -c "$(basename $PACKAGE_DIR)" | xz > "$TAR_FILENAME.xz"
