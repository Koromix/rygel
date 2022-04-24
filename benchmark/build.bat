@echo off

setlocal
cd %~dp0\..

echo Install all dependencies...
call npm install

echo Building dependencies...
call node ../cnoke/cnoke.js
call node ../cnoke/cnoke.js -C test

echo Building raylib_cc (Clang)...
clang -O2 -std=c++17 -Wall -I. -DNDEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE benchmark\raylib_cc.cc vendor\libcc\libcc.cc -o benchmark\raylib_cc.exe test\build\raylib.lib -lws2_32 -ladvapi32 -lshell32 -lole32
copy test\build\raylib.dll benchmark\raylib.dll
