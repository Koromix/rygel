#!/bin/sh -e

cd $(dirname $0)
echo "Building raylib_c..."

gcc -std=gnu99 -Wall -Wl,-rpath,$(realpath $(pwd)/../build) -fPIC raylib_c.c -o raylib_c ../build/raylib.so -lm
