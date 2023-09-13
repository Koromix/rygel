# Polymorphic arguments

## Input polymorphism

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

## Output buffers

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

See [decoding variables](variables.md#decode-to-js-values) for more information about the decode function.
