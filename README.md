# Table of contents

- [Introduction](#introduction)
- [Installation](#installation)
  * [Windows](#windows)
  * [Other platforms](#other-platforms)
- [Get started](#get-started)
- [Tests](#tests)

# Introduction

Koffi is a fast and easy-to-use FFI module for Node.js, with support for complex data types such as structs.

The following platforms __are supported__ at the moment:

* Windows x86 *(cdecl, stdcall)*
* Windows x86_64
* Linux x86
* Linux x86_64
* Linux ARM32+VFP Little Endian
* Linux ARM64 Little Endian

The following platforms are __not yet supported__ but will be soon:

* macOS x86_64
* macOS ARM64
* FreeBSD/NetBSD/OpenBSD x86_64
* FreeBSD/NetBSD/OpenBSD ARM64

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

This examples illustrates how to use Koffi with a Raylib shared library:

```js
const koffi = require('build/koffi.node');

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

// Fix the path to Raylib DLL
let raylib = koffi.load('Raylib.dll', {
    InitWindow: ['void', ['int', 'int', 'string']],
    SetTargetFPS: ['void', ['int']],
    GetScreenWidth: ['int', []],
    GetScreenHeight: ['int', []],
    ClearBackground: ['void', [Color]],
    BeginDrawing: ['void', []],
    EndDrawing: ['void', []],
    WindowShouldClose: ['bool', []],
    GetFontDefault: [Font, []],
    MeasureTextEx: [Vector2, [Font, 'string', 'float', 'float']],
    DrawTextEx: ['void', [Font, 'string', Vector2, 'float', 'float', Color]]
});

raylib.InitWindow(800, 600, 'Test Raylib');
raylib.SetTargetFPS(60);

let angle = 0;

while (!raylib.WindowShouldClose()) {
    raylib.BeginDrawing();
    raylib.ClearBackground({ r: 0, g: 0, b: 0, a: 255 }); // black

    let win_width = raylib.GetScreenWidth();
    let win_height = raylib.GetScreenHeight();

    let text = 'Hello World!';
    let text_width = raylib.MeasureTextEx(raylib.GetFontDefault(), text, 32, 1).x;

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

    raylib.DrawTextEx(raylib.GetFontDefault(), text, pos, 32, 1, color);

    raylib.EndDrawing();

    angle += Math.PI / 180;
}

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

And be patient, this can be pretty slow for emulated machines.

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
