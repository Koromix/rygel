#!/bin/sh -e

cd "$(dirname $0)"

rm -rf windows_arm64
mkdir windows_arm64

XWIN=../../vendor/xwin/xwin_$(uname -m)

$XWIN --cache-dir windows_arm64/cache --accept-license --arch aarch64 download
$XWIN --cache-dir windows_arm64/cache --accept-license --arch aarch64 unpack
$XWIN --cache-dir windows_arm64/cache --accept-license --arch aarch64 splat --output windows_arm64 --include-debug-libs --use-winsysroot-style

(cd 'windows_arm64/VC/Tools/MSVC' && ln -s *.*.*.* Current)
(cd 'windows_arm64/Windows Kits/10/Lib' && ln -s *.*.* Current)
ln -s 'Windows Kits' windows_arm64/Kits

rm -rf windows_arm64/cache
