#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../sysroots/windows_x64
mkdir ../sysroots/windows_x64

XWIN=../../vendor/xwin/xwin_$(uname -m)

$XWIN --cache-dir ../sysroots/windows_x64/cache --accept-license --arch x86_64 download
$XWIN --cache-dir ../sysroots/windows_x64/cache --accept-license --arch x86_64 unpack
$XWIN --cache-dir ../sysroots/windows_x64/cache --accept-license --arch x86_64 splat --output ../sysroots/windows_x64 --include-debug-libs --use-winsysroot-style

(cd '../sysroots/windows_x64/VC/Tools/MSVC' && ln -s *.*.*.* Current)
(cd '../sysroots/windows_x64/Windows Kits/10/Lib' && ln -s *.*.* Current)
ln -s 'Windows Kits' ../sysroots/windows_x64/Kits

rm -rf ../sysroots/windows_x64/cache
