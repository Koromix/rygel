#!/bin/sh

set -e

cd "$(dirname $0)"
cd ../../../..

./bootstrap.sh

mkdir -p bin/Packages/goupile/src

rm -f bin/Packages/goupile/src/bin/Shared/FelixCache.txt
./felix -pDebug -O bin/Packages/goupile/src/bin felix goupile

directories=$(grep -E -o '[|"](lib/native|lib/web|src|vendor)/[a-zA-Z0-9_\-]+/' < bin/Packages/goupile/src/bin/Shared/FelixCache.txt | cut -c 2- | sort | uniq)
sources="bootstrap.sh bootstrap.bat FelixBuild.ini FelixBuild.ini.presets $directories"
version=$(./felix -pDebug -O bin/Packages/goupile/src/bin --run goupile --version | awk -F'[ _]' "/^goupile/ {print \$2}")

rm -rf bin/Packages/goupile/src/tmp
mkdir -p bin/Packages/goupile/src/tmp/goupile-$version/

cp -r --parents $sources bin/Packages/goupile/src/tmp/goupile-$version/
cp src/goupile/*.md bin/Packages/goupile/src/tmp/goupile-$version/
