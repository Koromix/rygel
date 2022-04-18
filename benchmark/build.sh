#!/bin/sh -e

cd $(dirname $0)/..

echo "Install all dependencies..."
npm install

echo "Building dependencies..."
node ../cnoke/cnoke.js

echo "Building raylib_c..."
gcc -std=gnu99 -Wall -Wl,-rpath,$(realpath $(pwd)/build) -fPIC benchmark/raylib_c.c -o benchmark/raylib_c build/raylib.so -lm
