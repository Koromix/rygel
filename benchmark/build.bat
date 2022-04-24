@echo off

setlocal
cd %~dp0\..

echo Install all dependencies...
call npm install

echo Building dependencies...
call node ../cnoke/cnoke.js
call node ../cnoke/cnoke.js -C test

echo Building raylib_c (Clang)...
clang -O2 -std=gnu99 -Wall benchmark\raylib_c.c -o benchmark\raylib_c.exe test\build\raylib.lib
copy test\build\raylib.dll benchmark\raylib.dll
