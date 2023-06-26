#!/bin/sh -e

cd /io

apt install -y libudev-dev

# Fix git error about dubious repository ownership
git config --global safe.directory '*'

./bootstrap.sh
./felix -pFast -O bin/Packages/meestic/debian/bin meestic MeesticGui
