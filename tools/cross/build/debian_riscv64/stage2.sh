#!/bin/sh -e

cd /

ARCH=riscv64
HOSTNAME=debian-$ARCH
TARGET=target
DEST=dest
SYSROOT=sysroot
PACKAGES="curl wget htop dfc sudo nodejs build-essential git ninja-build clang lld gdb cmake ssh libx11-dev libxi-dev libgl-dev libxrandr-dev libxcursor-dev libxinerama-dev ccache vim xvfb xauth"
SUITE=trixie

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

chroot $TARGET adduser --gecos "Debian user,,," --disabled-password debian
echo "root:root" | chroot $TARGET chpasswd
echo "debian:debian" | chroot $TARGET chpasswd
chroot $TARGET adduser debian sudo
sed -i $TARGET/etc/sudoers -re 's/^%sudo.*/%sudo ALL=(ALL:ALL) NOPASSWD: ALL/g'

chroot $TARGET apt install -y opensbi u-boot-menu u-boot-qemu
chroot $TARGET ln -sf /dev/null /etc/systemd/system/serial-getty@hvc0.service
cat >> $TARGET/etc/default/u-boot <<EOF
U_BOOT_PARAMETERS="rw noquiet root=LABEL=rootfs"
U_BOOT_FDT_DIR="noexist"
EOF
chroot $TARGET u-boot-update
chroot $TARGET update-initramfs -k all -c

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

mkdir $SYSROOT/usr
cp -a $TARGET/usr/include $SYSROOT/usr/include
cp -a $TARGET/usr/lib $SYSROOT/usr/lib
ln -s ./usr/lib $SYSROOT/lib
symlinks -cr $TARGET/usr

cp -L $TARGET/usr/lib/riscv64-linux-gnu/opensbi/generic/fw_jump.bin $DEST/fw_jump.bin
cp -L $TARGET/usr/lib/u-boot/qemu-riscv64_smode/uboot.elf $DEST/uboot.elf
tar -cz -S -f $DEST/disk.tar.gz -C $TARGET .
sha256sum $DEST/disk.tar.gz | cut -d ' ' -f 1 > $DEST/VERSION
virt-make-fs --format=qcow2 --size=24G --partition=gpt --type=ext4 --label=rootfs $DEST/disk.tar.gz $DEST/disk.qcow2
qemu-img snapshot -c base $DEST/disk.qcow2
rm $DEST/disk.tar.gz
