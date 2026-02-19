#!/bin/sh -e

cd /

ARCH=i386
TARGET=target
DEST=dest
SYSROOT=sysroot
PACKAGES="build-essential clang lld libx11-dev libxi-dev libgl-dev libxrandr-dev libxcursor-dev libxinerama-dev libglx-dev"
SUITE=bookworm

export DEBIAN_FRONTEND=noninteractive

mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc
/usr/lib/systemd/systemd-binfmt

mkdir -p $TARGET $DEST

fakeroot qemu-debootstrap \
    --arch=$ARCH \
    $SUITE $TARGET

mount --bind /dev $TARGET/dev
mount --bind /dev/pts $TARGET/dev/pts
mount --bind /proc $TARGET/proc
mount --bind /sys $TARGET/sys
mount --bind /run $TARGET/run

chroot $TARGET apt update -y
chroot $TARGET apt install -y $PACKAGES

umount $TARGET/run
umount $TARGET/sys
umount $TARGET/proc
umount $TARGET/dev/pts
umount $TARGET/dev

mkdir $SYSROOT/usr
cp -a $TARGET/usr/include $SYSROOT/usr/include
cp -a $TARGET/usr/lib $SYSROOT/usr/lib
ln -s ./usr/lib $SYSROOT/lib
symlinks -cr $TARGET/usr
