#!/bin/sh -e

cd $(dirname $0)/..

echo "Install all dependencies..."
npm install

echo "Building dependencies..."
node ../cnoke/cnoke.js
node ../cnoke/cnoke.js -C test

echo "Building raylib_c..."
gcc -O2 -std=gnu99 -Wall -Wl,-rpath,$(realpath $(pwd)/test/build) -fPIC benchmark/raylib_c.c -o benchmark/raylib_c test/build/raylib.so -lm -pthread -ldl
