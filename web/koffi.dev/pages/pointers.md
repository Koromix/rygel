# How pointers are used

In C, pointer arguments are used for differenty purposes. It is important to distinguish these use cases because Koffi provides different ways to deal with each of them:

- **Struct pointers**: Use of struct pointers by C libraries fall in two cases: avoid (potentially) expensive copies, and to let the function change struct contents (output or input/output arguments).
- **Opaque pointers**: the library does not expose the contents of the structs, and only provides you with a pointer to it (e.g. `FILE *`). Only the functions provided by the library can do something with this pointer, in Koffi we call this an opaque type. This is usually done for ABI-stability reason, and to prevent library users from messing directly with library internals.
- **Pointers to primitive types**: This is more rare, and generally used for output or input/output arguments. The Win32 API has a lot of these.
- **Arrays**: in C, you dynamically-sized arrays are usually passed to functions with pointers, either NULL-terminated (or any other sentinel value) or with an additional length argument.

# Pointer types

## Struct pointers

The following Win32 example uses `GetCursorPos()` (with an output parameter) to retrieve and show the current cursor position.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('user32.dll');

// Type declarations
const POINT = koffi.struct('POINT', {
    x: 'long',
    y: 'long'
});

// Functions declarations
const GetCursorPos = lib.func('int __stdcall GetCursorPos(_Out_ POINT *pos)');

// Get and show cursor position
let pos = {};
if (!GetCursorPos(pos))
    throw new Error('Failed to get cursor position');
console.log(pos);
```

## Opaque pointers

Some C libraries use handles, which behave as pointers to opaque structs. An example of this is the HANDLE type in the Win32 API. If you want to reproduce this behavior, you can define a **named pointer type** to an opaque type, like so:

```js
const HANDLE = koffi.pointer('HANDLE', koffi.opaque());

// And now you get to use it this way:
const GetHandleInformation = lib.func('bool __stdcall GetHandleInformation(HANDLE h, _Out_ uint32_t *flags)');
const CloseHandle = lib.func('bool __stdcall CloseHandle(HANDLE h)');
```

## Pointer to primitive types

In javascript, it is not possible to pass a primitive value by reference to another function. This means that you cannot call a function and expect it to modify the value of one of its number or string parameter.

However, arrays and objects (among others) are reference type values. Assigning an array or an object from one variable to another does not invole any copy. Instead, as the following example illustrates, the new variable references the same array as the first:

```js
let list1 = [1, 2];
let list2 = list1;

list2[1] = 42;

console.log(list1); // Prints [1, 42]
```

All of this means that C functions that are expected to modify their primitive output values (such as an `int *` parameter) cannot be used directly. However, thanks to Koffi's transparent array support, you can use Javascript arrays to approximate reference semantics with single-element arrays.

Below, you can find an example of an addition function where the result is stored in an `int *` input/output parameter and how to use this function from Koffi.

```c
void AddInt(int *dest, int add)
{
    *dest += add;
}
```

You can simply pass a single-element array as the first argument:

```js
const AddInt = lib.func('void AddInt(_Inout_ int *dest, int add)');

let sum = [36];
AddInt(sum, 6);

console.log(sum[0]); // Prints 42
```

## Dynamic arrays

In C, dynamically-sized arrays are usually passed around as pointers. The length is either passed as an additional argument, or inferred from the array content itself, for example with a terminating sentinel value (such as a NULL pointers in the case of an array of strings).

Koffi can translate JS arrays and TypedArrays to pointer arguments. However, because C does not have a proper notion of dynamically-sized arrays (fat pointers), you need to provide the length or the sentinel value yourself depending on the API.

Here is a simple example of a C function taking a NULL-terminated list of strings as input, to calculate the total length of all strings.

```c
// Build with: clang -fPIC -o length.so -shared length.c -Wall -O2

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int64_t ComputeTotalLength(const char **strings)
{
    int64_t total = 0;

    for (const char **ptr = strings; *ptr; ptr++) {
        const char *str = *ptr;
        total += strlen(str);
    }

    return total;
}
```

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('./length.so');

const ComputeTotalLength = lib.func('int64_t ComputeTotalLength(const char **strings)');

let strings = ['Get', 'Total', 'Length', null];
let total = ComputeTotalLength(strings);

console.log(total); // Prints 14
```

By default, just like for objects, array arguments are copied from JS to C but not vice-versa. You can however change the direction as documented in the section on [output parameters](output).

# Handling void pointers

Many C functions use `void *` parameters in order to pass polymorphic objects and arrays, meaning that the data format changes can change depending on one other argument, or on some kind of struct tag member.

Koffi provides two features to deal with this:

- You can use `koffi.as(value, type)` to tell Koffi what kind of type is actually expected, as shown in the example below.
- Buffers and typed JS arrays can be used as values in place everywhere a pointer is expected. See [dynamic arrays](#dynamic-arrays) for more information, for input or output.

The example below shows the use of `koffi.as()` to read the header of a PNG file with `fread()` directly to a JS object.

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

# Unwrap pointers

You can use `koffi.address(ptr)` on a pointer to get the numeric value as a [BigInt object](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt).
