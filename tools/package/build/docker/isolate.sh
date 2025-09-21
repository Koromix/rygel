#!/bin/sh

set -e

binary=$1
dest=$2

loader=$(ldd $binary | awk -F '=> ' '{gsub(/^[ \t]+/, "", $1); print $1}' | awk '/^\// {print $1}')
libraries=$(ldd $binary | awk -F '=> ' '{print $2}' | awk '/.+/ {print $1}')

mkdir $dest
cp $binary $dest/
cp $loader $dest/ld-linux
cp -L $libraries $dest/

name=$(basename $dest)
patchelf --set-interpreter /$name/ld-linux --set-rpath /$name $dest/$(basename $binary)
