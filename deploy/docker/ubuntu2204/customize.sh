#!/bin/bash -e

apt update
apt install -y build-essential git cmake ninja-build pkg-config libudev-dev qt6-base-dev qt6-base-dev-tools gdb debhelper dh-make devscripts
