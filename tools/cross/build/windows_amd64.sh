#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../sysroots/windows_amd64
mkdir ../sysroots/windows_amd64

XWIN=../../vendor/xwin/xwin_$(uname -m)

$XWIN --cache-dir ../sysroots/windows_amd64/cache --accept-license --arch x86_64 download
$XWIN --cache-dir ../sysroots/windows_amd64/cache --accept-license --arch x86_64 unpack
$XWIN --cache-dir ../sysroots/windows_amd64/cache --accept-license --arch x86_64 splat --output ../sysroots/windows_amd64 --include-debug-libs --use-winsysroot-style

(cd '../sysroots/windows_amd64/VC/Tools/MSVC' && ln -s *.*.*.* Current)
(cd '../sysroots/windows_amd64/Windows Kits/10/Lib' && ln -s *.*.* Current)
ln -s 'Windows Kits' ../sysroots/windows_amd64/Kits

rm -rf ../sysroots/windows_amd64/cache
