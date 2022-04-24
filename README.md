# Table of contents

- [Introduction](#introduction)
- [Installation](#installation)
  * [Windows](#windows)
  * [Other platforms](#other-platforms)
- [Get started](#get-started)
- [Tests](#tests)
- [Benchmarks](#benchmarks)
  * [atoi results](#atoi-results)
  * [Raylib results](#raylib-results)

# Introduction

Koffi is a fast and easy-to-use FFI module for Node.js, with support for complex data types such as structs.

The following platforms __are officially supported and tested__ at the moment:

* Windows x86 *(cdecl, stdcall, fastcall)*
* Windows x86_64
* Linux x86
* Linux x86_64
* Linux ARM32+VFP Little Endian
* Linux ARM64 Little Endian
* FreeBSD x86
* FreeBSD x86_64
* FreeBSD ARM64 Little Endian
* macOS x86_64

The following platforms will __soon be officially supported__:

* macOS ARM64

The following platforms __may be supported__ but are not tested:

* NetBSD x86_64
* NetBSD ARM64
* OpenBSD x86_64
* OpenBSD ARM64

This is still in development, bugs are to expected. More tests will come in the near future.

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

const InitWindow = lib.cdecl('InitWindow', 'void', ['int', 'int', 'string']);
const SetTargetFPS = lib.cdecl('SetTargetFPS', 'void', ['int']);
const GetScreenWidth = lib.cdecl('GetScreenWidth', 'int', []);
const GetScreenHeight = lib.cdecl('GetScreenHeight', 'int', []);
const ClearBackground = lib.cdecl('ClearBackground', 'void', [Color]);
const BeginDrawing = lib.cdecl('BeginDrawing', 'void', []);
const EndDrawing = lib.cdecl('EndDrawing', 'void', []);
const WindowShouldClose = lib.cdecl('WindowShouldClose', 'bool', []);
const GetFontDefault = lib.cdecl('GetFontDefault', Font, []);
const MeasureTextEx = lib.cdecl('MeasureTextEx', Vector2, [Font, 'string', 'float', 'float']);
const DrawTextEx = lib.cdecl('DrawTextEx', 'void', [Font, 'string', Vector2, 'float', 'float', Color]);

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

These machines are not included directly in this repository (for license and size reasons), but they are available here: https://koromix.dev/files/koffi/

For example, if you want to run the tests on Debian ARM64, run the following commands:

```sh
cd luigi/koffi/test/
wget -q -O- https://koromix.dev/files/koffi/qemu_debian_arm64.tar.zst | zstd -d | tar xv
sha256sum -c --ignore-missing registry/sha256sum.txt
```

Note that the machine disk content may change each time the machine runs, so the checksum test will fail once a machine has been used at least once.

And now you can run the tests with:

```sh
node test.js # Several options are available, use --help
```

And be patient, this can be pretty slow for emulated machines. The Linux machines have and use ccache to build Koffi, so subsequent build steps will get much more tolerable.

By default, machines are started and stopped for each test. But you can start the machines ahead of time and run the tests multiple times instead:

```sh
node test.js start # Start the machines
node test.js # Test (without shutting down)
node test.js # Test again
node test.js stop # Stop everything
```

You can also restrict the test to a subset of machines:

```sh
# Full test cycle
node test.js test debian_x64 debian_i386

# Separate start, test, shutdown
node test.js start debian_x64 debian_i386
node test.js test debian_x64 debian_i386
node test.js stop
```

Finally, you can join a running machine with SSH with the following shortcut, if you need to do some debugging or any other manual procedure:

```sh
node test.js ssh debian_i386
```

Each machine is configured to run a VNC server available locally, which you can use to access the display, using KRDC or any other compatible viewer. Use the `info` command to get the VNC port.

```sh
node test.js info debian_x64
```

# Benchmarks

At this stage, two benchmarks are implemented:
* The first one is based around repeated calls to atoi, in three implementations: with Koffi, with node-ffi-napi and with C code (with dlsym / GetProcAddress). This is a simple function, thus the JS and FFI overhead is clearly visible.
* The second one is based around Raylib, and will execute much more heavier functions repeatdly. Also in three versions: Koffi, node-ffi-napi and C code.

In order to run it, go to `koffi/benchmark` and run `../../cnoke/cnoke.js` (or `node ..\..\cnoke\cnoke.js` on Windows) before doing anything else.

Once this is done, you can execute each implementation, e.g. `build/atoi_cc 20000000` or `./atoi_koffi.js 20000000`.

## atoi results

Here are some results from 2022-04-24 on my Linux machine (AMD® Ryzen™ 7 5800H 16G):

```sh
$ build/atoi_cc 20000000
Iterations: 20000000
Time: 0.24s

$ ./atoi_koffi.js 20000000
Iterations: 20000000
Time: 2.41s

# Note: the Node-FFI version does a few setTimeout calls to force the GC to run (around 20
# for the example below), without which Node will consume all memory because the GC never appears
# to run, or not enough. It's not ideal but on the other hand it counts as another limitation
# to Node-FFI performance.
$ ./atoi_node_ffi.js 20000000
Iterations: 20000000
Time: 640.49s
```

And on my Windows machine (Intel® Core™ i5-4460 16G):

```sh
$ build\atoi_cc.exe 20000000
Iterations: 20000000
Time: 0.66s

$ node atoi_koffi.js 20000000
Iterations: 20000000
Time: 4.81s

$ node atoi_node_ffi.js 20000000
Iterations: 20000000
Time: 491.99s
```

## Raylib results

Here are some results from 2022-04-24 on my Linux machine (AMD® Ryzen™ 7 5800H 16G):

```sh
$ build/raylib_cc 100
Iterations: 100
Time: 4.14s

$ ./raylib_koffi.js 100
Iterations: 100
Time: 6.25s

$ ./raylib_node_ffi.js 100
Iterations: 100
Time: 27.13s
```

And on my Windows machine (Intel® Core™ i5-4460 16G):

```sh
$ build\raylib_cc.exe 100
Iterations: 100
Time: 10.53s

$ node raylib_koffi.js 100
Iterations: 100
Time: 14.60s

$ node raylib_node_ffi.js 100
Iterations: 100
Time: 44.97s
```
