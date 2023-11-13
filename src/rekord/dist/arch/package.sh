#!/bin/sh -e

cd "$(dirname $0)/../../../.."

./bootstrap.sh
./felix -pDebug rekord

VERSION=$(bin/Debug/rekord --version | awk '/^rekord/ {print $2}' | sed 's/[_\-]/\./g')
DATE=$(git show -s --format=%ci | LANG=en_US xargs -0 -n1 date "+%a, %d %b %Y %H:%M:%S %z" -d)
PACKAGE_DIR=bin/Packages/rekord/arch

rm -rf $PACKAGE_DIR/pkg
mkdir -p $PACKAGE_DIR $PACKAGE_DIR/pkg

docker build -t rygel/arch deploy/docker/arch
docker run -t -i --rm -v $(pwd):/io rygel/arch /io/src/rekord/dist/arch/build.sh

install -D -m0755 bin/Packages/rekord/arch/bin/rekord $PACKAGE_DIR/pkg/rekord

sed -e "s/VERSION/$VERSION/" src/rekord/dist/arch/PKGBUILD > $PACKAGE_DIR/pkg/PKGBUILD

chmod 0777 $PACKAGE_DIR/pkg
docker run -t -i --rm -v $(pwd):/io -w /io/$PACKAGE_DIR/pkg -u nobody rygel/arch makepkg
cp $PACKAGE_DIR/pkg/rekord-*.pkg.tar.zst $PACKAGE_DIR/../

./bootstrap.sh
