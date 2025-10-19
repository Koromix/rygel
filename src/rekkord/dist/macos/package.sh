#!/bin/sh

set -e
cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug rekkord

VERSION=$(bin/Debug/rekkord --version | awk -F'[ _]' '/^rekkord/ {print $2}')
PACKAGE_DIR=bin/Packages/rekkord/macos

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg

install -m0644 src/rekkord/README.md $PACKAGE_DIR/pkg/README.md
install -m0644 LICENSE.txt $PACKAGE_DIR/pkg/LICENSE.txt

for arch in x86_64 ARM64; do
    ./felix -pFast --host=$arch -O bin/Fast_${arch} rekkord

    dmg_filename=bin/Packages/rekkord_${VERSION}_macos_${arch}.dmg

    rm -f "$dmg_filename"
    install -m0755 bin/Fast_${arch}/rekkord "$PACKAGE_DIR/pkg/rekkord"
    hdiutil create -srcfolder "$PACKAGE_DIR/pkg" -volname "rekkord" "$dmg_filename"
done
