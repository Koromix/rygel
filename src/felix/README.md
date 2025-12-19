# Overview

Felix is a small build system made for this repository:

- Parallel builds, with easy PCH support for faster builds
- Cross-compiler support for various features: optimization levels, intrisics flags, sanitizers, etc.
- JS bundling support based on esbuild
- Easy support for linking with static and dynamic Qt

It is made for my needs, and the needs of this repository, and is thus quite opinionated:

- No support for system-wide libraries (I prefer vendorized dependencies)
- No integration with different build systems such as CMake

# Get started

## Bootstrap

The repository contains bootstrap scripts that can be used to build felix using nothing but a standard shell (or batch) script and a C++ compiler (GCC, Clang or MSVC).

On Linux, macOS and other POSIX systems, run the bootstrapt scrip with:

```sh
./bootstrap.sh
```

On Windows, use the batch script instead:

```sh
bootstrap.bat
```

## Using felix

Run felix anywhere inside the repository to build all projects:

```sh
# Build all targets
./felix

# Build specific target
./felix target
```

Build presets are provided and configured in `FelixBuild.ini.presets`. This repository configures several presets such as:

- *Debug* (default)
- *ASan / TSan / UBSan / AUBSan*: debug builds with one or more sanitizers
- *Fast*
- *LTO*
- *Paranoid*: LTO builds with various additional mitigations: CFI, ZeroInit, etc.

Use `felix -p PRESET [targets]` to use a specific preset.

# Building Qt apps

## Dynamic Qt

If Qt in installed globally (Linux package) or in a standard location such as `C:/Qt` (Windows) or `/Users/<user>/Qt` (macOS), felix will find it aumatically.

If it does not, set the environment variable QMAKE_PATH to point to the `qmake` binary.

```sh
export QMAKE_PATH=/path/to/qmake
felix QtApp
```

## Static Qt on Linux

### Build static Qt

```sh
wget https://download.qt.io/archive/qt/6.10/6.10.1/single/qt-everywhere-src-6.10.1.tar.xz

tar xvf qt-everywhere-src-6.10.1.tar.xz
cd qt-everywhere-src-6.10.1

./configure -release -static -opensource -prefix $HOME/Qt/Static/6.10.1 \
            -submodules qtbase,qtsvg -no-icu -no-cups -qt-pcre -qt-zlib -qt-libpng -qt-libjpeg \
            -xcb -fontconfig

cmake --build . --parallel
cmake --install .
```

### Run felix

```sh
export QMAKE_PATH=$HOME/Qt/Static/6.10.1/bin/qmake
felix QtApp
```

## Static Qt on macOS

### Build static Qt

```sh
wget https://download.qt.io/archive/qt/6.10/6.10.1/single/qt-everywhere-src-6.10.1.tar.xz

tar xvf qt-everywhere-src-6.10.1.tar.xz
cd qt-everywhere-src-6.10.1

./configure -release -static -opensource -prefix $HOME/Qt/Static/6.10.1 \
            -submodules qtbase,qtsvg -no-cups -no-freetype -qt-pcre \
            -no-icu -no-harfbuzz -no-pkg-config

cmake --build . --parallel
cmake --install .
```

### Run felix

```sh
export QMAKE_PATH=$HOME/Qt/Static/6.10.1/bin/qmake
felix QtApp
```

## Static Qt on Windows

### Build static Qt

Start by downloading the Qt source: https://download.qt.io/archive/qt/6.10/6.10.1/single/qt-everywhere-src-6.10.1.zip

Extract it, open a command prompt and execute the following commands:

```sh
cd qt-everywhere-src-6.10.1

configure -release -static -opensource -platform win32-msvc -prefix C:/Qt/Static/6.10.1 ^
          -submodules qtbase,qtsvg -static-runtime -no-opengl -no-harfbuzz -no-icu -no-cups -qt-pcre -qt-zlib ^
          -qt-freetype -qt-libpng -qt-libjpeg

cmake --build . --parallel
cmake --install .
```

### Run felix

```sh
set QMAKE_PATH=C:/Qt/Static/6.10.1/bin/qmake
felix QtApp
```
