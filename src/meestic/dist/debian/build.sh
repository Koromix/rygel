#!/bin/sh -e

cd /io

apt install -y libudev-dev

# Fix git error about dubious repository ownership
git config --global safe.directory '*'

./bootstrap.sh --no_user
./felix -pFast --no_user -O bin/Packages/meestic/debian/bin meestic MeesticTray
