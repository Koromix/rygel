#!/bin/sh -e

export DEBIAN_FRONTEND=noninteractive

apt update -y
apt install -y fakeroot debootstrap libguestfs-tools symlinks qemu-user-static
