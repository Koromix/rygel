#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug meestic

VERSION=$(bin/Debug/meestic --version | awk '/^meestic/ {print $2}')
DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/meestic/arch

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg

docker build -t rygel/arch deploy/docker/arch
docker run -t -i --rm -v $(pwd):/io rygel/arch /io/src/meestic/dist/arch/build.sh

install -D -m0755 bin/Packages/meestic/arch/bin/meestic $PACKAGE_DIR/pkg/meestic
install -D -m0755 bin/Packages/meestic/arch/bin/MeesticTray $PACKAGE_DIR/pkg/MeesticTray
install -D -m0644 src/meestic/dist/arch/meestic.service $PACKAGE_DIR/pkg/meestic.service
install -D -m0644 src/meestic/dist/arch/meestic.ini $PACKAGE_DIR/pkg/meestic.ini
install -D -m0644 src/meestic/dist/arch/MeesticTray.desktop $PACKAGE_DIR/pkg/MeesticTray.desktop
install -D -m0644 src/meestic/images/meestic.png $PACKAGE_DIR/pkg/MeesticTray.png

sed -e "s/VERSION/$VERSION/" src/meestic/dist/arch/PKGBUILD > $PACKAGE_DIR/pkg/PKGBUILD

chmod 0777 $PACKAGE_DIR/pkg
docker run -t -i --rm -v $(pwd):/io -w /io/$PACKAGE_DIR/pkg -u nobody rygel/arch makepkg
cp $PACKAGE_DIR/pkg/meestic-*.pkg.tar.zst $PACKAGE_DIR/../

./bootstrap.sh
