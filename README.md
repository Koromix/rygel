# Table of contents

- [Introduction](#introduction)
- [Benchmarks](#benchmarks)
  * [atoi results](#atoi-results)
  * [Raylib results](#raylib-results)
- [Installation](#installation)
  * [Windows](#windows)
  * [Other platforms](#other-platforms)
- [Get started](#get-started)
- [Tests](#tests)

# Introduction

Koffi is a fast and easy-to-use FFI module for Node.js, with support for complex data types such as structs.

The following platforms __are officially supported and tested__ at the moment:

Platoform | Architecture                     | JS to C | C to JS (callback)
--------- | -------------------------------- | ------- | ------------------
Windows   | x86 (cdecl, stdcall, fastcall)   | 🟩      | 🟥
Windows   | x86_64                           | 🟩      | 🟥
Linux     | x86                              | 🟩      | 🟥
Linux     | x86_64                           | 🟩      | 🟥
Linux     | ARM32+VFP Little Endian          | 🟩      | 🟥
Linux     | ARM64 Little Endian              | 🟩      | 🟥
FreeBSD   | x86                              | 🟩      | 🟥
FreeBSD   | x86_64                           | 🟩      | 🟥
FreeBSD   | ARM64 Little Endian              | 🟩      | 🟥
macOS     | x86_64                           | 🟩      | 🟥
macOS     | ARM64 (M1) Little Endian         | 🟩      | 🟥
NetBSD    | x86_64                           | 🟧      | 🟥
NetBSD    | ARM64 Little Endian              | 🟧      | 🟥
OpenBSD   | x86_64                           | 🟧      | 🟥
OpenBSD   | ARM64 Little Endian              | 🟧      | 🟥

🟩 Tested, fully opertional
🟧 May work, but not actively tested
🟥 Does not work yet

This is still in development, bugs are to expected. More tests will come in the near future.

# Benchmarks

In order to run it, go to `koffi/benchmark` and run `../../cnoke/cnoke.js` (or `node ..\..\cnoke\cnoke.js` on Windows) before doing anything else.

Once this is done, you can execute each implementation, e.g. `build/atoi_cc` or `./atoi_koffi.js`. You can optionally define a custom number of iterations, e.g. `./atoi_koffi.js 10000000`.

## atoi results

This test is based around repeated calls to a simple standard C function atoi, and has three implementations:
- the first one is the reference, it calls atoi through an N-API module, and is close to the theoretical limit of a perfect (no overhead) Node.js > C FFI implementation.
- the second one calls atoi through Koffi
- the third one uses the official Node.js FFI implementation, node-ffi-napi

Because atoi is a small call, the FFI overhead is clearly visible.

### Linux

The results below were measured on my x86_64 Linux machine (AMD® Ryzen™ 7 5800H 16G):

Benchmark     | Iterations | Total time  | Overhead
------------- | ---------- | ----------- | ----------
atoi_napi     | 20000000   | 1.10s       | (baseline)
atoi_koffi    | 20000000   | 1.91s       | x1.73
atoi_node_ffi | 20000000   | 640.49s     | x582

### Windows

The results below were measured on my x86_64 Windows machine (AMD® Ryzen™ 7 5800H 16G):

Benchmark     | Iterations | Total time  | Overhead
------------- | ---------- | ----------- | ----------
atoi_napi     | 20000000   | 1.94s       | (baseline)
atoi_koffi    | 20000000   | 3.15s       | x1.62
atoi_node_ffi | 20000000   | 640.49s     | x242

## Raylib results

This benchmark uses the CPU-based image drawing functions in Raylib. The calls are much heavier than in the atoi benchmark, thus the FFI overhead is reduced. In this implemenetation, the baseline is a full C++ version of the code.

### Linux

The results below were measured on my x86_64 Linux machine (AMD® Ryzen™ 7 5800H 16G):

Benchmark       | Iterations | Total time  | Overhead
--------------- | ---------- | ----------- | ----------
raylib_cc       | 100        | 4.14s       | (baseline)
raylib_koffi    | 100        | 6.25s       | x1.51
raylib_node_ffi | 100        | 27.13s      | x6.55

### Windows

The results below were measured on my x86_64 Windows machine (AMD® Ryzen™ 7 5800H 16G):

Benchmark       | Iterations | Total time  | Overhead
--------------- | ---------- | ----------- | ----------
raylib_cc       | 100        | 8.39s       | (baseline)
raylib_koffi    | 100        | 11.51s      | x1.37
raylib_node_ffi | 100        | 31.47s      | x3.8

# Installation

## Windows

First, make sure the following dependencies are met:

* The "Desktop development with C++" workload from [Visual Studio 2022 or 2019](https://visualstudio.microsoft.com/downloads/) or the "C++ build tools" workload from the [Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022), with the default optional components.
* [CMake meta build system](https://cmake.org/)
* [Node 16 LTS](https://nodejs.org/), but a newer version should work too

Once this is done, run this command from the project root:

```sh
npm install koffi
```

## Other platforms

Make sure the following dependencies are met:

* `gcc` and `g++` >= 8.3 or newer
* GNU Make 3.81 or newer
* [CMake meta build system](https://cmake.org/)

Once these dependencies are met, simply run the follow command:

```sh
npm install koffi
```

# Get started

This section assumes you know how to build C shared libraries.

## Raylib example

This examples illustrates how to use Koffi with a Raylib shared library:

```js
const koffi = require('koffi');

const Color = koffi.struct('Color', {
    r: 'uchar',
    g: 'uchar',
    b: 'uchar',
    a: 'uchar'
});

const Image = koffi.struct('Image', {
    data: koffi.pointer('void'),
    width: 'int',
    height: 'int',
    mipmaps: 'int',
    format: 'int'
});

const GlyphInfo = koffi.struct('GlyphInfo', {
    value: 'int',
    offsetX: 'int',
    offsetY: 'int',
    advanceX: 'int',
    image: Image
});

const Vector2 = koffi.struct('Vector2', {
    x: 'float',
    y: 'float'
});

const Rectangle = koffi.struct('Rectangle', {
    x: 'float',
    y: 'float',
    width: 'float',
    height: 'float'
});

const Texture = koffi.struct('Texture', {
    id: 'uint',
    width: 'int',
    height: 'int',
    mipmaps: 'int',
    format: 'int'
});

const Font = koffi.struct('Font', {
    baseSize: 'int',
    glyphCount: 'int',
    glyphPadding: 'int',
    texture: Texture,
    recs: koffi.pointer(Rectangle),
    glyphs: koffi.pointer(GlyphInfo)
});

// Fix the path to Raylib DLL if needed
let lib = koffi.load('build/raylib' + koffi.extension);

// Classic function declaration
const InitWindow = lib.func('InitWindow', 'void', ['int', 'int', 'string']);
const SetTargetFPS = lib.func('SetTargetFPS', 'void', ['int']);
const GetScreenWidth = lib.func('GetScreenWidth', 'int', []);
const GetScreenHeight = lib.func('GetScreenHeight', 'int', []);
const ClearBackground = lib.func('ClearBackground', 'void', [Color]);

// Prototype parser
const BeginDrawing = lib.func('void BeginDrawing()');
const EndDrawing = lib.func('void EndDrawing()');
const WindowShouldClose = lib.func('void WindowShouldClose(bool)');
const GetFontDefault = lib.func('Font GetFontDefault()');
const MeasureTextEx = lib.func('Vector2 MeasureTextEx(Font, const char *, float, float)');
const DrawTextEx = lib.func('void DrawTextEx(Font font, const char *text, Vector2 pos, float size, float spacing, Color tint)');

InitWindow(800, 600, 'Test Raylib');
SetTargetFPS(60);

let angle = 0;

while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground({ r: 0, g: 0, b: 0, a: 255 }); // black

    let win_width = GetScreenWidth();
    let win_height = GetScreenHeight();

    let text = 'Hello World!';
    let text_width = MeasureTextEx(GetFontDefault(), text, 32, 1).x;

    let color = {
        r: 127.5 + 127.5 * Math.sin(angle),
        g: 127.5 + 127.5 * Math.sin(angle + Math.PI / 2),
        b: 127.5 + 127.5 * Math.sin(angle + Math.PI),
        a: 255
    };
    let pos = {
        x: (win_width / 2 - text_width / 2) + 120 * Math.cos(angle - Math.PI / 2),
        y: (win_height / 2 - 16) + 120 * Math.sin(angle - Math.PI / 2)
    };

    DrawTextEx(GetFontDefault(), text, pos, 32, 1, color);

    EndDrawing();

    angle += Math.PI / 180;
}

```

## Win32 stdcall example

```js
const koffi = require('koffi');

let lib = koffi.load('user32.dll');

const MessageBoxA = lib.stdcall('MessageBoxA', 'int', ['void *', 'string', 'string', 'uint']);
const MB_ICONINFORMATION = 0x40;

MessageBoxA(null, 'Hello', 'Foobar', MB_ICONINFORMATION);
```

# Tests

Koffi is tested on multiple architectures using emulated (accelerated when possible) QEMU machines. First, you need to install qemu packages, such as `qemu-system` (or even `qemu-system-gui`) on Ubuntu.

These machines are not included directly in this repository (for license and size reasons), but they are available here: https://koromix.dev/files/machines/

For example, if you want to run the tests on Debian ARM64, run the following commands:

```sh
cd luigi/koffi/qemu/
wget -q -O- https://koromix.dev/files/machines/qemu_debian_arm64.tar.zst | zstd -d | tar xv
sha256sum -c --ignore-missing registry/sha256sum.txt
```

Note that the machine disk content may change each time the machine runs, so the checksum test will fail once a machine has been used at least once.

And now you can run the tests with:

```sh
node qemu.js # Several options are available, use --help
```

And be patient, this can be pretty slow for emulated machines. The Linux machines have and use ccache to build Koffi, so subsequent build steps will get much more tolerable.

By default, machines are started and stopped for each test. But you can start the machines ahead of time and run the tests multiple times instead:

```sh
node qemu.js start # Start the machines
node qemu.js # Test (without shutting down)
node qemu.js # Test again
node qemu.js stop # Stop everything
```

You can also restrict the test to a subset of machines:

```sh
# Full test cycle
node qemu.js test debian_x64 debian_i386

# Separate start, test, shutdown
node qemu.js start debian_x64 debian_i386
node qemu.js test debian_x64 debian_i386
node qemu.js stop
```

Finally, you can join a running machine with SSH with the following shortcut, if you need to do some debugging or any other manual procedure:

```sh
node qemu.js ssh debian_i386
```

Each machine is configured to run a VNC server available locally, which you can use to access the display, using KRDC or any other compatible viewer. Use the `info` command to get the VNC port.

```sh
node qemu.js info debian_x64
```