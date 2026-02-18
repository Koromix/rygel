#!/bin/sh -e

cd "$(dirname $0)"

SYSROOT=../../sysroots/windows_i386
XWIN=../../../../vendor/xwin/xwin_$(uname -m)

rm -rf $SYSROOT
mkdir $SYSROOT

$XWIN --cache-dir $SYSROOT/cache --accept-license --arch x86 download
$XWIN --cache-dir $SYSROOT/cache --accept-license --arch x86 unpack
$XWIN --cache-dir $SYSROOT/cache --accept-license --arch x86 splat --output $SYSROOT --include-debug-libs --use-winsysroot-style

(cd "$SYSROOT/VC/Tools/MSVC" && ln -s *.*.*.* Current)
(cd "$SYSROOT/Windows Kits/10/Lib" && ln -s *.*.* Current)
ln -s 'Windows Kits' $SYSROOT/Kits

rm -rf $SYSROOT/cache
