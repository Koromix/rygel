#!/bin/sh -e

cd /io

apt install -y libudev-dev qt6-base-dev qt6-base-dev-tools pkg-config gdb

# Fix git error about dubious repository ownership
git config --global safe.directory '*'

./bootstrap.sh --no_user
./felix -pFast --no_user -O bin/Packages/tytools/debian/bin tycmd TyCommander TyUploader
