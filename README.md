# Table of contents

- [Introduction](#introduction)
- [Get started](#get-started)
- [Extra features](#extra-features)
  * [Type information](#type-information)
  * [C arrays](#c-arrays)
  * [Variadic functions](#variadic-functions)
  * [Asynchronous calls](#asynchronous-calls)
  * [Callbacks](#callbacks)
- [Benchmarks](#benchmarks)
  * [atoi results](#atoi-results)
  * [Raylib results](#raylib-results)
- [Tests](#tests)
- [Compilation](#compilation)
  * [Windows](#windows-1)
  * [Other platforms](#other-platforms)

# Introduction

Koffi is a fast and easy-to-use FFI module for Node.js, with support for primitive and aggregate data types (structs), both by reference (pointer) and by value.

The following features are planned in the near future:

* 1.2: C to JS callbacks
* 1.3: RISC-V support (32 and 64 bit)
* 1.4: Type parser

The following platforms __are officially supported and tested__ at the moment:

Platform  | Architecture                     | Sync calls | Async calls | Callbacks | Pre-built binary
--------- | -------------------------------- | ---------- | ----------- | --------- | ----------------
Windows   | x86 (cdecl, stdcall, fastcall)   | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
Windows   | x86_64                           | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
Linux     | x86                              | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
Linux     | x86_64                           | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
Linux     | ARM32+VFP Little Endian          | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
Linux     | ARM32 without VFP Little Endian  | 🟧 Maybe    | 🟧 Maybe     | 🟥 No      | 🟩 Yes
Linux     | ARM64 Little Endian              | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
FreeBSD   | x86                              | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
FreeBSD   | x86_64                           | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
FreeBSD   | ARM64 Little Endian              | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
macOS     | x86_64                           | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟩 Yes
macOS     | ARM64 (M1) Little Endian         | 🟩 Yes      | 🟩 Yes       | 🟥 No      | 🟥 No
OpenBSD   | x86_64                           | 🟧 Maybe    | 🟧 Maybe     | 🟥 No      | 🟥 No
OpenBSD   | x86                              | 🟧 Maybe    | 🟧 Maybe     | 🟥 No      | 🟥 No
OpenBSD   | ARM64 Little Endian              | 🟧 Maybe    | 🟧 Maybe     | 🟥 No      | 🟥 No
NetBSD    | x86_64                           | 🟧 Maybe    | 🟧 Maybe     | 🟥 No      | 🟥 No
NetBSD    | x86                              | 🟧 Maybe    | 🟧 Maybe     | 🟥 No      | 🟥 No
NetBSD    | ARM64 Little Endian              | 🟧 Maybe    | 🟧 Maybe     | 🟥 No      | 🟥 No

🟩 Tested, fully operational
🟧 May work, but not actively tested
🟥 Does not work yet

This is still in development, bugs are to be expected. More tests will come in the near future.

# Get started

Once you have installed koffi with `npm install koffi`, you can start by loading it this way:

```js
const koffi = require('koffi');
```

Below you can find three examples:

* The first one runs on Linux. The functions are declared with the C-like prototype language.
* The second one runs on Windows, and uses the node-ffi like syntax to declare functions.
* The third one is more complex and uses Raylib to animate "Hello World" in a window.

## Small Linux example

```js
const koffi = require('koffi');
const lib = koffi.load('libc.so.6');

// Declare types
const timeval = koffi.struct('timeval', {
    tv_sec: 'unsigned int',
    tv_usec: 'unsigned int'
});
const timezone = koffi.struct('timezone', {
    tz_minuteswest: 'int',
    tz_dsttime: 'int'
});

// Declare functions
const gettimeofday = lib.func('int gettimeofday(_Out_ timeval *tv, _Out_ timezone *tz)');
const printf = lib.func('int printf(const char *format, ...)');

let tv = {};
let tz = {};
gettimeofday(tv, tz);

printf('Hello World!, it is: %d\n', 'int', tv.tv_sec);
console.log(tz);
```

## Small Windows example

```js
const koffi = require('koffi');
const lib = koffi.load('user32.dll');

const MessageBoxA = lib.stdcall('MessageBoxA', 'int', ['void *', 'string', 'string', 'uint']);
const MB_ICONINFORMATION = 0x40;

MessageBoxA(null, 'Hello', 'Foobar', MB_ICONINFORMATION);
```

## Raylib example

This section assumes you know how to build C shared libraries, such as Raylib. You may need to fix the path to the library before you can do anything.

```js
const koffi = require('koffi');
let lib = koffi.load('raylib.dll'); // Fix path if needed

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

# Extra features

## Type information

Koffi exposes three functions to explore type information:
- `koffi.sizeof(type)` to get the size of a type
- `koffi.alignof(type)` to get the alignment of a type
- `koffi.introspect(type)` to get the definition of a type (only for structs for now)

## C arrays

Fixed-size arrays are declared with `koffi.array(type, length)`. Just like in C, they cannot be passed
as functions parameters (they degenerate to pointers), or returned by value. You can however embed them in struct types.

### JS typed arrays

Special rules apply for arrays of primitive integer and float types (uint32_t, double, etc...):
- When converting from JS to C, Koffi can take a normal Array (e.g. `[1, 2]`) or a TypedArray of the correct type (e.g. `Uint8Array` for an array of `uint8_t` numbers)
- When converting from C to JS (for return value or output parameters), Koffi will by default use a TypedArray. But you can change this behavior when you create the array type with the optional hint argument: `koffi.array('uint8_t', 64, 'array')`

See the example below:

```js
const koffi = require('koffi');

// Those two structs are exactly the same, only the array conversion hint is different
const Foo1 = koffi.struct('Foo', {
    i: 'int',
    a16: koffi.array('int16_t', 8)
});
const Foo2 = koffi.struct('Foo', {
    i: 'int',
    a16: koffi.array('int16_t', 8, 'array')
});

// Uses an hypothetical C function that just returns the struct passed as a parameter
const ReturnFoo1 = lib.func('Foo1 ReturnFoo(Foo1 p)');
const ReturnFoo2 = lib.func('Foo2 ReturnFoo(Foo2 p)');

console.log(ReturnFoo1({ i: 5, a16: [6, 8] })) // Prints { i: 5, a16: Int16Array(2) [6, 8] }
console.log(ReturnFoo2({ i: 5, a16: [6, 8] })) // Prints { i: 5, a16: [6, 8] }
```

### C strings

Koffi can also convert JS strings to fixed-sized arrays in the following cases:
- char (or int8_t) arrays are filled with the UTF-8 encoded string, truncated if needed. The buffer is always NUL-terminated.
- char16 (or int16_t) arrays are filled with the UTF-16 encoded string, truncated if needed. The buffer is always NUL-terminated.

The reverse case is also true, Koffi can convert a C fixed-size buffer to a JS string. Use the `string` array hint to do this (e.g. `koffi.array('char', 8, 'string')`).

## Variadic functions

Variadic functions are declared with an ellipsis as the last argument.

In order to call a variadic function, you must provide two Javascript arguments for each C parameter, the first one is the expected type and the second one is the value.

```js
const printf = lib.func('printf', 'int', ['string', '...']);

// The variadic arguments are: 6 (int), 8.5 (double), 'THE END' (const char *) 
printf('Integer %d, double %g, string %s', 'int', 6, 'double', 8.5, 'string', 'THE END');
```

## Asynchronous calls

You can issue asynchronous calls by calling the function through its async member. In this case, you need to provide a callback function as the last argument, with `(err, res)` parameters.

```js
const koffi = require('koffi');
const lib = koffi.load('libc.so.6');

const atoi = lib.func('int atoi(const char *str)');

atoi.async('1257', (err, res) => {
    console.log('Result:', res);
})
console.log('Hello World!');

// This program will print "Hello World!", and then "Result: 1257"
```

You can easily convert this callback-style async function to a promise-based version with `util.promisify()` from the Node.js standard library.

Variadic functions do not support async.

## Callbacks

Koffi does not yet support passing JS functions as callbacks. This is planned for version 1.2.

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

# Compilation

We provide prebuilt binaries, packaged in the NPM archive, so in most cases it should be as simple as `npm install koffi`. If you want to hack Koffi or use a specific platform, follow the instructions below.

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
