#!/bin/sh

set -e
cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug rekkord

VERSION=$(bin/Debug/rekkord --version | awk -F'[ _]' '/^rekkord/ {print $2}')
PACKAGE_DIR=bin/Packages/rekkord/macos/rekkord_${VERSION}/pkg
ROOT_DIR=bin/Packages/rekkord/macos/rekkord_${VERSION}/root
PKG_FILENAME=bin/Packages/rekkord_${VERSION}_macos.pkg
TAR_FILENAME=bin/Packages/rekkord_${VERSION}_macos.tar

rm -rf "$PACKAGE_DIR" "$ROOT_DIR"
mkdir -p "$PACKAGE_DIR" "$ROOT_DIR"

install -m0644 src/rekkord/README.md "$PACKAGE_DIR/README.md"
install -m0644 LICENSE.txt "$PACKAGE_DIR/LICENSE.txt"

./felix -pFast --host=x86_64 -O bin/x86_64 rekkord
./felix -pFast --host=ARM64 -O bin/ARM64 rekkord
lipo -create -output "$PACKAGE_DIR/rekkord" bin/x86_64/rekkord bin/ARM64/rekkord

mkdir -p "$ROOT_DIR/usr/local/bin" "$ROOT_DIR/usr/local/share/doc/rekkord"
install -m0755 "$PACKAGE_DIR/rekkord" "$ROOT_DIR/usr/local/bin/rekkord"
install -m0644 src/rekkord/README.md "$ROOT_DIR/usr/local/share/doc/rekkord/README.md"
install -m0644 LICENSE.txt "$ROOT_DIR/usr/local/share/doc/rekkord/LICENSE.txt"

rm -f "$PKG_FILENAME"
pkgbuild --root "$ROOT_DIR" --identifier org.rekkord --version $VERSION --install-location / "$PKG_FILENAME"
tar -C "$(dirname $PACKAGE_DIR)" -c "$(basename $PACKAGE_DIR)" | gzip > "$TAR_FILENAME.gz"
tar -C "$(dirname $PACKAGE_DIR)" -c "$(basename $PACKAGE_DIR)" | xz > "$TAR_FILENAME.xz"
