#!/bin/sh -e

export DEBIAN_FRONTEND=noninteractive

apt update -y
apt install -y qemu-user-static debootstrap libguestfs-tools symlinks
