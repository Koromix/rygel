# Overview

libhs is a C library to **enumerate and interact with USB HID and USB serial devices** and interact with them. It is:

- Single-file: one header is all you need to make it work.
- Public domain: use it, hack it, do whatever you want.
- Multiple platforms: Windows (≥ Vista), macOS (≥ 10.9) and Linux.
- Multiple compilers: MSVC (≥ 2015), GCC and Clang.
- Driverless: uses native OS-provided interfaces and does not require custom drivers.

You can [download libhs.h](https://codeberg.org/Koromix/libraries) from the Codeberg repository.

# Build

The `libhs.h` file provides both the interface and the implementation. To instantiate the implementation, `#define HS_IMPLEMENTATION` in *ONE* source file, before including libhs.h.

libhs depends on **a few OS-provided libraries** that you need to link:

OS                  | Dependencies
------------------- | --------------------------------------------------------------------------------
Windows (MSVC)      | Nothing to do, libhs uses `#pragma comment(lib)`
Windows (MinGW-w64) | Link `-luser32 -ladvapi32 -lsetupapi -lhid`
OSX (Clang)         | Link _CoreFoundation and IOKit_
Linux (GCC)         | Link `-ludev`

This library is developed as part of the TyTools project where you can find the original [libhs source code](https://codeberg.org/Koromix/rygel/src/branch/master/src/tytools/libhs). The amalgamated header file is automatically produced by CMake scripts.

Look at [Sean Barrett's excellent stb libraries](https://github.com/nothings/stb) for the reasoning behind this mode of distribution.

# License

All the code related to these programs is under **public domain**, you can do whatever you want with it. See the LICENSE file or [unlicense.org](https://unlicense.org/) more more information.
