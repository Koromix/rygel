@echo off

setlocal enableDelayedExpansion
cd %~dp0

set VERSION=0.14.10

curl -O https://registry.npmjs.org/esbuild-windows-64/-/esbuild-windows-64-%VERSION%.tgz
tar zx --strip-components=1 -f esbuild-windows-64-%VERSION%.tgz package/esbuild.exe
move /Y esbuild.exe esbuild_win_x64.exe

curl -O https://registry.npmjs.org/esbuild-windows-32/-/esbuild-windows-32-%VERSION%.tgz
tar zx --strip-components=1 -f esbuild-windows-32-%VERSION%.tgz package/esbuild.exe
move /Y esbuild.exe esbuild_win_ia32.exe

curl -O https://registry.npmjs.org/esbuild-darwin-64/-/esbuild-darwin-64-%VERSION%.tgz
tar zx --strip-components=2 -f esbuild-darwin-64-%VERSION%.tgz package/bin/esbuild
move /Y esbuild esbuild_mac_x64

curl -O https://registry.npmjs.org/esbuild-linux-64/-/esbuild-linux-64-%VERSION%.tgz
tar zx --strip-components=2 -f esbuild-linux-64-%VERSION%.tgz package/bin/esbuild
move /Y esbuild esbuild_linux_x64

curl -O https://registry.npmjs.org/esbuild-linux-arm64/-/esbuild-linux-arm64-%VERSION%.tgz
tar zx --strip-components=2 -f esbuild-linux-arm64-%VERSION%.tgz package/bin/esbuild
move /Y esbuild esbuild_linux_x64

del *.tgz
