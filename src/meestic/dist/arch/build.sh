#!/bin/sh -e

cd /io

# Fix git error about dubious repository ownership
git config --global safe.directory '*'

./bootstrap.sh
./felix -pFast -O bin/Packages/meestic/arch/bin meestic MeesticGui
