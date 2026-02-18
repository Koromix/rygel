#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../sysroots/windows_ia32
mkdir ../sysroots/windows_ia32

XWIN=../../vendor/xwin/xwin_$(uname -m)

$XWIN --cache-dir ../sysroots/windows_ia32/cache --accept-license --arch x86 download
$XWIN --cache-dir ../sysroots/windows_ia32/cache --accept-license --arch x86 unpack
$XWIN --cache-dir ../sysroots/windows_ia32/cache --accept-license --arch x86 splat --output ../sysroots/windows_ia32 --include-debug-libs --use-winsysroot-style

(cd '../sysroots/windows_ia32/VC/Tools/MSVC' && ln -s *.*.*.* Current)
(cd '../sysroots/windows_ia32/Windows Kits/10/Lib' && ln -s *.*.* Current)
ln -s 'Windows Kits' ../sysroots/windows_ia32/Kits

rm -rf ../sysroots/windows_ia32/cache
