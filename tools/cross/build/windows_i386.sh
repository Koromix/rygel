#!/bin/sh -e

cd "$(dirname $0)"

rm -rf ../sysroots/windows_i386
mkdir ../sysroots/windows_i386

XWIN=../../vendor/xwin/xwin_$(uname -m)

$XWIN --cache-dir ../sysroots/windows_i386/cache --accept-license --arch x86 download
$XWIN --cache-dir ../sysroots/windows_i386/cache --accept-license --arch x86 unpack
$XWIN --cache-dir ../sysroots/windows_i386/cache --accept-license --arch x86 splat --output ../sysroots/windows_i386 --include-debug-libs --use-winsysroot-style

(cd '../sysroots/windows_i386/VC/Tools/MSVC' && ln -s *.*.*.* Current)
(cd '../sysroots/windows_i386/Windows Kits/10/Lib' && ln -s *.*.* Current)
ln -s 'Windows Kits' ../sysroots/windows_i386/Kits

rm -rf ../sysroots/windows_i386/cache
