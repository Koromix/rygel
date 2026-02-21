#!/bin/sh -e

cd /

ARCH=amd64
HOSTNAME=debian-i386
TARGET=target
DEST=dest
PACKAGES="curl wget htop dfc sudo build-essential git ninja-build clang lld gdb lldb cmake ssh libx11-dev:i386 libxi-dev:i386 libgl-dev:i386 libxrandr-dev:i386 libxcursor-dev:i386 libxinerama-dev:i386 libglx-dev:i386 ccache vim xvfb xauth"
SUITE=bookworm

export DEBIAN_FRONTEND=noninteractive

mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc
/usr/lib/systemd/systemd-binfmt

mkdir -p $TARGET $DEST

fakeroot qemu-debootstrap \
    --arch=$ARCH \
    --include="linux-image-$ARCH,openssh-server" \
    $SUITE $TARGET

mount --bind /dev $TARGET/dev
mount --bind /dev/pts $TARGET/dev/pts
mount --bind /proc $TARGET/proc
mount --bind /sys $TARGET/sys
mount --bind /run $TARGET/run

echo "$HOSTNAME" > $TARGET/etc/hostname
echo "127.0.0.1 $HOSTNAME" >> $TARGET/etc/hosts

chroot $TARGET dpkg --add-architecture i386
chroot $TARGET apt update -y
chroot $TARGET apt install -y $PACKAGES
chroot $TARGET apt install -y nodejs:i386 gcc-multilib g++-multilib

chroot $TARGET adduser --gecos "Debian user,,," --disabled-password debian
echo "root:root" | chroot $TARGET chpasswd
echo "debian:debian" | chroot $TARGET chpasswd
chroot $TARGET adduser debian sudo
sed -i $TARGET/etc/sudoers -re 's/^%sudo.*/%sudo ALL=(ALL:ALL) NOPASSWD: ALL/g'

ln -s /dev/null $TARGET/etc/systemd/network/99-default.link
cp /host/interfaces $TARGET/etc/network/interfaces
cp /host/resolv.conf $TARGET/etc/resolv.conf

umount $TARGET/run
umount $TARGET/sys
umount $TARGET/proc
umount $TARGET/dev/pts
umount $TARGET/dev

rm -rf $TARGET/var/lib/apt/*
rm -rf $TARGET/var/cache/apt/*

cp -L $TARGET/vmlinuz $DEST/vmlinuz
cp -L $TARGET/initrd.img $DEST/initrd.img

tar -cz -S -f $DEST/disk.tar.gz -C $TARGET .
virt-make-fs --format=qcow2 --size=24G --partition=gpt --type=ext4 --label=rootfs $DEST/disk.tar.gz $DEST/disk.qcow2
qemu-img snapshot -c base $DEST/disk.qcow2
rm $DEST/disk.tar.gz
