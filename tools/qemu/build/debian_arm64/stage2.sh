#!/bin/sh -e

cd /

ARCH=arm64
HOSTNAME=debian-$ARCH
TARGET=target
DEST=dest
PACKAGES="curl wget htop dfc sudo build-essential git ninja-build gdb lldb cmake ssh libx11-dev libxi-dev libgl-dev libxrandr-dev libxcursor-dev libxinerama-dev ccache vim xvfb xauth"
SUITE=bullseye

export DEBIAN_FRONTEND=noninteractive

mount binfmt_misc -t binfmt_misc /proc/sys/fs/binfmt_misc

mkdir -p $TARGET $DEST

qemu-debootstrap \
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

chroot $TARGET apt update -y
chroot $TARGET apt install -y $PACKAGES

chroot $TARGET apt install -y lsb-release wget software-properties-common gnupg
curl -o $TARGET/llvm.sh https://apt.llvm.org/llvm.sh
chmod +x $TARGET/llvm.sh
chroot $TARGET /llvm.sh 21
rm $TARGET/llvm.sh

chroot $TARGET adduser --gecos "Debian user,,," --disabled-password debian
echo "root:root" | chroot $TARGET chpasswd
echo "debian:debian" | chroot $TARGET chpasswd
chroot $TARGET adduser debian sudo
sed -i $TARGET/etc/sudoers -re 's/^%sudo.*/%sudo ALL=(ALL:ALL) NOPASSWD: ALL/g'

mkdir $TARGET/opt/node
curl -L -o- https://nodejs.org/dist/v24.13.0/node-v24.13.0-linux-arm64.tar.xz | tar -xJ -C $TARGET/opt/node --strip-components=1
ln -s /opt/node/bin/node $TARGET/usr/local/bin/node
ln -s /opt/node/bin/npm $TARGET/usr/local/bin/npm

ln -s /dev/null $TARGET/etc/systemd/network/99-default.link
cp interfaces $TARGET/etc/network/interfaces
cp resolv.conf $TARGET/etc/resolv.conf

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
sha256sum $DEST/disk.tar.gz | cut -d ' ' -f 1 > $DEST/VERSION
virt-make-fs --format=qcow2 --size=24G --partition=gpt --type=ext4 --label=rootfs $DEST/disk.tar.gz $DEST/disk.qcow2
qemu-img snapshot -c base $DEST/disk.qcow2
rm $DEST/disk.tar.gz
