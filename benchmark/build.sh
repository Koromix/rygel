#!/bin/sh -e

cd $(dirname $0)/..

echo "Install all dependencies..."
npm install

echo "Building dependencies..."
node ../cnoke/cnoke.js
node ../cnoke/cnoke.js -C test

echo "Building raylib_cc..."
g++ -O2 -std=c++17 -Wall -I. -DNDEBUG -Wl,-rpath,$(realpath $(pwd)/test/build) -fPIC benchmark/raylib_cc.cc vendor/libcc/libcc.cc -o benchmark/raylib_cc test/build/raylib.so -lm -pthread -ldl
