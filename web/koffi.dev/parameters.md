# Special parameters

## Direction

By default, Koffi will only forward arguments from Javascript to C. However, many C functions use pointer arguments for output values, or input/output values.

## Output parameters

For simplicity, and because Javascript only has value semantics for primitive types, Koffi can marshal out (or in/out) two types of parameters:

- [Structs](types.md#struct-types) (to/from JS objects)
- [Unions](unions.md)
- [Opaque types](types.md#opaque-types)
- String buffers

In order to change an argument from input-only to output or input/output, use the following functions:

- `koffi.out()` on a pointer, e.g. `koffi.out(koffi.pointer(timeval))` (where timeval is a struct type)
- `koffi.inout()` for dual input/output parameters

The same can be done when declaring a function with a C-like prototype string, with the MSDN-like type qualifiers:

- `_Out_` for output parameters
- `_Inout_` for dual input/output parameters

### Primitive value

This Windows example enumerate all Chrome windows along with their PID and their title. The `GetWindowThreadProcessId()` function illustrates how to get a primitive value from an output argument.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const user32 = koffi.load('user32.dll');

const DWORD = koffi.alias('DWORD', 'uint32_t');
const HANDLE = koffi.pointer(koffi.opaque('HANDLE'));
const HWND = koffi.alias('HWND', HANDLE);

const FindWindowEx = user32.func('HWND __stdcall FindWindowExW(HWND hWndParent, HWND hWndChildAfter, const char16_t *lpszClass, const char16_t *lpszWindow)');
const GetWindowThreadProcessId = user32.func('DWORD __stdcall GetWindowThreadProcessId(HWND hWnd, _Out_ DWORD *lpdwProcessId)');
const GetWindowText = user32.func('int __stdcall GetWindowTextA(HWND hWnd, _Out_ uint8_t *lpString, int nMaxCount)');

for (let hwnd = null;;) {
    hwnd = FindWindowEx(0, hwnd, 'Chrome_WidgetWin_1', null);

    if (!hwnd)
        break;

    // Get PID
    let pid;
    {
        let ptr = [null];
        let tid = GetWindowThreadProcessId(hwnd, ptr);

        if (!tid) {
            // Maybe the process ended in-between?
            continue;
        }

        pid = ptr[0];
    }

    // Get window title
    let title;
    {
        let buf = Buffer.allocUnsafe(1024);
        let length = GetWindowText(hwnd, buf, buf.length);

        if (!length) {
            // Maybe the process ended in-between?
            continue;
        }

        title = koffi.decode(buf, 'char', length);
    }

    console.log({ PID: pid, Title: title });
}
```

### Struct example

This example calls the POSIX function `gettimeofday()`, and uses the prototype-like syntax.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('libc.so.6');

const timeval = koffi.struct('timeval', {
    tv_sec: 'unsigned int',
    tv_usec: 'unsigned int'
});
const timezone = koffi.struct('timezone', {
    tz_minuteswest: 'int',
    tz_dsttime: 'int'
});

// The _Out_ qualifiers instruct Koffi to marshal out the values
const gettimeofday = lib.func('int gettimeofday(_Out_ timeval *tv, _Out_ timezone *tz)');

let tv = {};
gettimeofday(tv, null);

console.log(tv);
```

### Opaque type example

This example opens an in-memory SQLite database, and uses the node-ffi-style function declaration syntax.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('sqlite3.so');

const sqlite3 = koffi.opaque('sqlite3');

// Use koffi.out() on a double pointer to copy out (from C to JS) after the call
const sqlite3_open_v2 = lib.func('sqlite3_open_v2', 'int', ['str', koffi.out(koffi.pointer(sqlite3, 2)), 'int', 'str']);
const sqlite3_close_v2 = lib.func('sqlite3_close_v2', 'int', [koffi.pointer(sqlite3)]);

const SQLITE_OPEN_READWRITE = 0x2;
const SQLITE_OPEN_CREATE = 0x4;

let out = [null];
if (sqlite3_open_v2(':memory:', out, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, null) != 0)
    throw new Error('Failed to open database');
let db = out[0];

sqlite3_close_v2(db);
```

### String buffer example

*New in Koffi 2.2*

This example calls a C function to concatenate two strings to a pre-allocated string buffer. Since JS strings are immutable, you must pass an array with a single string instead.

```c
void ConcatToBuffer(const char *str1, const char *str2, char *out)
{
    size_t len = 0;

    for (size_t i = 0; str1[i]; i++) {
        out[len++] = str1[i];
    }
    for (size_t i = 0; str2[i]; i++) {
        out[len++] = str2[i];
    }

    out[len] = 0;
}
```

```js
const ConcatToBuffer = lib.func('void ConcatToBuffer(const char *str1, const char *str2, _Out_ char *out)');

let str1 = 'Hello ';
let str2 = 'Friends!';

// We need to reserve space for the output buffer! Including the NUL terminator
// because ConcatToBuffer() expects so, but Koffi can convert back to a JS string
// without it (if we reserve the right size).
let out = ['\0'.repeat(str1.length + str2.length + 1)];

ConcatToBuffer(str1, str2, out);

console.log(out[0]);
```

## Polymorphic parameters

### Input polymorphism

*New in Koffi 2.1*

Many C functions use `void *` parameters in order to pass polymorphic objects and arrays, meaning that the data format changes can change depending on one other argument, or on some kind of struct tag member.

Koffi provides two features to deal with this:

- Buffers and typed JS arrays can be used as values in place everywhere a pointer is expected. See [dynamic arrays](pointers.md#array-pointers-dynamic-arrays) for more information, for input or output.
- You can use `koffi.as(value, type)` to tell Koffi what kind of type is actually expected.

The example below shows the use of `koffi.as()` to read the header of a PNG file with `fread()`.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('libc.so.6');

const FILE = koffi.opaque('FILE');

const PngHeader = koffi.pack('PngHeader', {
    signature: koffi.array('uint8_t', 8),
    ihdr: koffi.pack({
        length: 'uint32_be_t',
        chunk: koffi.array('char', 4),
        width: 'uint32_be_t',
        height: 'uint32_be_t',
        depth: 'uint8_t',
        color: 'uint8_t',
        compression: 'uint8_t',
        filter: 'uint8_t',
        interlace: 'uint8_t',
        crc: 'uint32_be_t'
    })
});

const fopen = lib.func('FILE *fopen(const char *path, const char *mode)');
const fclose = lib.func('int fclose(FILE *fp)');
const fread = lib.func('size_t fread(_Out_ void *ptr, size_t size, size_t nmemb, FILE *fp)');

let filename = process.argv[2];
if (filename == null)
    throw new Error('Usage: node png.js <image.png>');

let hdr = {};
{
    let fp = fopen(filename, 'rb');
    if (!fp)
        throw new Error(`Failed to open '${filename}'`);

    try {
        let len = fread(koffi.as(hdr, 'PngHeader *'), 1, koffi.sizeof(PngHeader), fp);
        if (len < koffi.sizeof(PngHeader))
            throw new Error('Failed to read PNG header');
    } finally {
        fclose(fp);
    }
}

console.log('PNG header:', hdr);
```

### Output buffers

*New in Koffi 2.3*

You can use buffers and typed arrays for output (and input/output) pointer parameters. Simply pass the buffer as an argument and the native function will receive a pointer to its contents.

Once the native function returns, you can decode the content with `koffi.decode(value, type)` as in the following example:

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('libc.so.6');

const Vec3 = koffi.struct('Vec3', {
    x: 'float32',
    y: 'float32',
    z: 'float32'
})

const memcpy = lib.func('void *memcpy(_Out_ void *dest, const void *src, size_t size)');

let vec1 = { x: 1, y: 2, z: 3 };
let vec2 = null;

// Copy the vector in a convoluted way through memcpy
{
    let src = koffi.as(vec1, 'Vec3 *');
    let dest = Buffer.allocUnsafe(koffi.sizeof(Vec3));

    memcpy(dest, src, koffi.sizeof(Vec3));

    vec2 = koffi.decode(dest, Vec3);
}

// CHange vector1, leaving copy alone
[vec1.x, vec1.y, vec1.z] = [vec1.z, vec1.y, vec1.x];

console.log(vec1); // { x: 3, y: 2, z: 1 }
console.log(vec2); // { x: 1, y: 2, z: 3 }
```

See [pointer arguments](callbacks.md#decoding-pointer-arguments) for more information about the decode function.

## Heap-allocated values

*New in Koffi 2.0*

Some C functions return heap-allocated values directly or through output parameters. While Koffi automatically converts values from C to JS (to a string or an object), it does not know when something needs to be freed, or how.

For opaque types, such as FILE, this does not matter because you will explicitly call `fclose()` on them. But some values (such as strings) get implicitly converted by Koffi, and you lose access to the original pointer. This creates a leak if the string is heap-allocated.

To avoid this, you can instruct Koffi to call a function on the original pointer once the conversion is done, by creating a **disposable type** with `koffi.dispose(name, type, func)`. This creates a type derived from another type, the only difference being that *func* gets called with the original pointer once the value has been converted and is not needed anymore.

The *name* can be omitted to create an anonymous disposable type. If *func* is omitted or is null, Koffi will use `koffi.free(ptr)` (which calls the standard C library *free* function under the hood).

```js
const AnonHeapStr = koffi.disposable('str'); // Anonymous type (cannot be used in function prototypes)
const NamedHeapStr = koffi.disposable('HeapStr', 'str'); // Same thing, but named so usable in function prototypes
const ExplicitFree = koffi.disposable('HeapStr16', 'str16', koffi.free); // You can specify any other JS function
```

The following example illustrates the use of a disposable type derived from *str*.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('libc.so.6');

const HeapStr = koffi.disposable('str');
const strdup = lib.cdecl('strdup', HeapStr, ['str']);

let copy = strdup('Hello!');
console.log(copy); // Prints Hello!
```

When you declare functions with the [prototype-like syntax](functions.md#definition-syntax), you can either use named disposable types or use the '!' shortcut qualifier with compatibles types, as shown in the example below. This qualifier creates an anonymous disposable type that calls `koffi.free(ptr)`.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('libc.so.6');

// You can also use: const strdup = lib.func('const char *! strdup(const char *str)')
const strdup = lib.func('str! strdup(const char *str)');

let copy = strdup('World!');
console.log(copy); // Prints World!
```

Disposable types can only be created from pointer or string types.

```{warning}
Be careful on Windows: if your shared library uses a different CRT (such as msvcrt), the memory could have been allocated by a different malloc/free implementation or heap, resulting in undefined behavior if you use `koffi.free()`.
```
