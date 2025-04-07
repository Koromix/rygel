#!/bin/sh

set -e
cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pParanoid tycommander tyuploader tycmd

VERSION=$(bin/Fast/tycmd --version | awk -F'[ _]' '/^tycmd/ {print $2}')
PACKAGE_DIR=bin/Packages/tytools/macos
DMG_FILENAME=bin/Packages/tytools_${VERSION}_macos.dmg

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg

install -m0755 bin/Fast/tycmd $PACKAGE_DIR/pkg/tycmd
cp -rp bin/Fast/TyCommander.app $PACKAGE_DIR/pkg/TyCommander.app
cp -rp bin/Fast/TyUploader.app $PACKAGE_DIR/pkg/TyUploader.app
ln -s /Applications $PACKAGE_DIR/pkg/Applications
install -m0644 src/tytools/README.md $PACKAGE_DIR/pkg/README.md
install -m0644 src/tytools/LICENSE.txt $PACKAGE_DIR/pkg/LICENSE.txt

rm -f $DMG_FILENAME
hdiutil create -srcfolder "$PACKAGE_DIR/pkg" -volname "TyTools" $DMG_FILENAME
