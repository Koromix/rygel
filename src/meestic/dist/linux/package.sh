#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pFast meestic MeesticGui

VERSION=$(./felix --version | awk '/^felix/ {print $2}')
PACKAGE_DIR=bin/Package/meestic_${VERSION}
PACKAGE_FILENAME=${PACKAGE_DIR}.install

rm -rf $PACKAGE_DIR
mkdir -p $PACKAGE_DIR

install -m0755 bin/Fast/meestic ${PACKAGE_DIR}/
install -m0755 bin/Fast/MeesticGui ${PACKAGE_DIR}/
install -m0644 src/meestic/README.md ${PACKAGE_DIR}/
install -m0755 src/meestic/dist/linux/* ${PACKAGE_DIR}/
install -m0644 src/meestic/images/meestic.png ${PACKAGE_DIR}/MeesticGui.png

makeself ${PACKAGE_DIR}/ ${PACKAGE_FILENAME} meestic ./install.sh
