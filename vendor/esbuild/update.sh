#!/bin/sh -e

cd $(dirname $0)

REPOSITORY=https://github.com/evanw/esbuild.git
VERSION=$1

rm -rf src
git clone --depth 1 --branch v$VERSION $REPOSITORY src
rm -rf src/.git src/.github

SYS_REQUIRE=$(awk '/require golang.org\/x\/sys/ { print $2 " " $3 }' < src/go.mod)
SYS_COMMIT=$(echo $SYS_REQUIRE | awk -F- '{ print $3 }')

git clone https://go.googlesource.com/sys src/sys
git -C src/sys checkout $SYS_COMMIT
rm -rf src/sys/.git

echo "replace $SYS_REQUIRE => ./sys" >> src/go.mod
echo -n "" > src/go.sum

rm -rf wasm
npm install esbuild-wasm@$VERSION
mv node_modules/esbuild-wasm wasm
rm -rf node_modules
rm package.json package-lock.json

curl -O https://registry.npmjs.org/@esbuild/win32-x64/-/win32-x64-$VERSION.tgz
tar zx --strip-components=1 -f win32-x64-$VERSION.tgz package/esbuild.exe
mv esbuild.exe bin/esbuild_windows_x64.exe
touch bin/esbuild_windows_x64.exe
chmod -x bin/esbuild_windows_x64.exe

curl -O https://registry.npmjs.org/@esbuild/linux-x64/-/linux-x64-$VERSION.tgz
tar zx --strip-components=2 -f linux-x64-$VERSION.tgz package/bin/esbuild
mv esbuild bin/esbuild_linux_x64
touch bin/esbuild_linux_x64
chmod +x bin/esbuild_linux_x64

curl -O https://registry.npmjs.org/@esbuild/linux-arm64/-/linux-arm64-$VERSION.tgz
tar zx --strip-components=2 -f linux-arm64-$VERSION.tgz package/bin/esbuild
mv esbuild bin/esbuild_linux_arm64
touch bin/esbuild_linux_arm64
chmod +x bin/esbuild_linux_arm64

curl -O https://registry.npmjs.org/@esbuild/darwin-x64/-/darwin-x64-$VERSION.tgz
tar zx --strip-components=2 -f darwin-x64-$VERSION.tgz package/bin/esbuild
mv esbuild bin/esbuild_macos_x64
touch bin/esbuild_macos_x64
chmod +x bin/esbuild_macos_x64

curl -O https://registry.npmjs.org/@esbuild/darwin-arm64/-/darwin-arm64-$VERSION.tgz
tar zx --strip-components=2 -f darwin-arm64-$VERSION.tgz package/bin/esbuild
mv esbuild bin/esbuild_macos_arm64
touch bin/esbuild_macos_arm64
chmod +x bin/esbuild_macos_arm64

rm *.tgz
