# Compilation on Windows

## MSVC 20xx 64-bit with static MSVCRT

These instructions have been tested with *Qt 6.6.1*, they will probably **not work for
Qt versions < 6.6**. Even if they work, you may not be able to link TyTools correctly.

Download qtbase source from https://download.qt.io/official_releases/qt/6.6/6.6.1/submodules/qtbase-everywhere-src-6.6.1.zip

Extract the directory inside it to "rygel/src/tytools/dist/qt6" and rename "qtbase-everywhere-src-6.6.1" to
"x86_64-win32-msvc-mt". Open the "VS20xx x64 Native Tools Command Prompt" and cd to
this directory.

```batch
cd x86_64-win32-msvc-mt
REM Now we are in rygel/src/tytools/dist/qt6/x86_64-win32-msvc-mt
configure -platform win32-msvc ^
    -opensource ^
    -confirm-license ^
    -static ^
    -static-runtime ^
    -release ^
    -nomake examples ^
    -nomake tests ^
    -no-opengl ^
    -no-harfbuzz ^
    -no-icu ^
    -no-cups ^
    -qt-pcre ^
    -qt-zlib ^
    -qt-freetype ^
    -qt-libpng ^
    -qt-libjpeg
nmake
```

Unfortunately Qt static builds are fragile and cannot be moved around. You will need to rebuild Qt
if you move your project.

# Mac OS X / Clang 64-bit

These instructions have been tested with *Qt 6.1.1*, they will probably **not work for
Qt versions < 6.6**. Even if they work, you may not be able to link TyTools correctly.

A recent version of XCode must be installed.

Download qtbase source from https://download.qt.io/official_releases/qt/6.6/6.6.1/submodules/qtbase-everywhere-src-6.6.1.tar.xz

Extract the directory inside it to "rygel/src/tytools/dist/qt6" and rename "qtbase-everywhere-src-6.6.1" to
"x86_64-darwin-clang". Open a command prompt and go to that directory.

```sh
cd x86_64-darwin-clang
# Now we are in rygel/src/tytools/dist/qt6/x86_64-darwin-clang
./configure -platform macx-clang \
    -opensource \
    -confirm-license \
    -static \
    -release \
    -nomake examples \
    -nomake tests \
    -no-pkg-config \
    -no-harfbuzz \
    -no-icu \
    -no-cups \
    -no-freetype \
    -qt-pcre
make
```

Unfortunately Qt static builds are fragile and cannot be moved around. You will need to rebuild Qt
if you move your project.
