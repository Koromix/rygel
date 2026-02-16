#!/bin/sh -e

cd "$(dirname $0)"

rm -rf windows_amd64
mkdir windows_amd64

XWIN=../../vendor/xwin/xwin_$(uname -m)

$XWIN --cache-dir windows_amd64/cache --accept-license  download
$XWIN --cache-dir windows_amd64/cache --accept-license unpack
$XWIN --cache-dir windows_amd64/cache --accept-license splat --output windows_amd64 --include-debug-libs --use-winsysroot-style

(cd 'windows_amd64/VC/Tools/MSVC' && ln -s *.*.*.* Current)
(cd 'windows_amd64/Windows Kits/10/Lib' && ln -s *.*.* Current)
ln -s 'Windows Kits' windows_amd64/Kits

rm -rf windows_amd64/cache
