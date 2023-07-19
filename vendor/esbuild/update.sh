#!/bin/sh -e

cd $(dirname $0)

VERSION=0.18.14

curl -O https://registry.npmjs.org/@esbuild/win32-x64/-/win32-x64-$VERSION.tgz
tar zx --strip-components=1 -f win32-x64-$VERSION.tgz package/esbuild.exe
mv esbuild.exe esbuild_windows_x64.exe
chmod -x esbuild_windows_x64.exe

curl -O https://registry.npmjs.org/@esbuild/win32-ia32/-/win32-ia32-$VERSION.tgz
tar zx --strip-components=1 -f win32-ia32-$VERSION.tgz package/esbuild.exe
mv esbuild.exe esbuild_windows_x86.exe
chmod -x esbuild_windows_x86.exe

curl -O https://registry.npmjs.org/@esbuild/darwin-x64/-/darwin-x64-$VERSION.tgz
tar zx --strip-components=2 -f darwin-x64-$VERSION.tgz package/bin/esbuild
mv esbuild esbuild_macos_x64
chmod +x esbuild_macos_x64

curl -O https://registry.npmjs.org/@esbuild/linux-x64/-/linux-x64-$VERSION.tgz
tar zx --strip-components=2 -f linux-x64-$VERSION.tgz package/bin/esbuild
mv esbuild esbuild_linux_x64
chmod +x esbuild_linux_x64

curl -O https://registry.npmjs.org/@esbuild/linux-arm64/-/linux-arm64-$VERSION.tgz
tar zx --strip-components=2 -f linux-arm64-$VERSION.tgz package/bin/esbuild
mv esbuild esbuild_linux_arm64
chmod +x esbuild_linux_arm64

rm *.tgz
