# Data types

## Primitive types

### Standard types

While the C standard allows for variation in the size of most integer types, Koffi enforces the same definition for most primitive types, listed below:

JS type          | C type                        | Bytes | Signedness | Note
---------------- | ----------------------------- | ----- | ---------- | ---------------------------
Undefined        | void                          | 0     |            | Only valid as a return type
Number (integer) | int8, int8_t                  | 1     | Signed     |
Number (integer) | uint8, uint8_t                | 1     | Unsigned   |
Number (integer) | char                          | 1     | Signed     |
Number (integer) | uchar, unsigned char          | 1     | Unsigned   |
Number (integer) | char16, char16_t              | 2     | Signed     |
Number (integer) | int16, int16_t                | 2     | Signed     |
Number (integer) | uint16, uint16_t              | 2     | Unsigned   |
Number (integer) | short                         | 2     | Signed     |
Number (integer) | ushort, unsigned short        | 2     | Unsigned   |
Number (integer) | int32, int32_t                | 4     | Signed     |
Number (integer) | uint32, uint32_t              | 4     | Unsigned   |
Number (integer) | int                           | 4     | Signed     |
Number (integer) | uint, unsigned int            | 4     | Unsigned   |
Number (integer) | int64, int64_t                | 8     | Signed     |
Number (integer) | uint64, uint64_t              | 8     | Unsigned   |
Number (integer) | longlong, long long           | 8     | Signed     |
Number (integer) | ulonglong, unsigned long long | 8     | Unsigned   |
Number (float)   | float32                       | 4     |            |
Number (float)   | float64                       | 8     |            |
Number (float)   | float                         | 4     |            |
Number (float)   | double                        | 8     |            |

Koffi also accepts BigInt values when converting from JS to C integers. If the value exceeds the range of the C type, Koffi will convert the number to an undefined value. In the reverse direction, BigInt values are automatically used when needed for big 64-bit integers.

Koffi defines a few more types that can change size depending on the OS and the architecture:

JS type          | C type           | Signedness | Note
---------------- | ---------------- | ---------- | ------------------------------------------------
Boolean          | bool             |            | Usually one byte
Number (integer) | long             | Signed     | 4 or 8 bytes depending on platform (LP64, LLP64)
Number (integer) | ulong            | Unsigned   | 4 or 8 bytes depending on platform (LP64, LLP64)
Number (integer) | unsigned long    | Unsigned   | 4 or 8 bytes depending on platform (LP64, LLP64)
Number (integer) | intptr           | Signed     | 4 or 8 bytes depending on register width
Number (integer) | intptr_t         | Signed     | 4 or 8 bytes depending on register width
Number (integer) | uintptr          | Unsigned   | 4 or 8 bytes depending on register width
Number (integer) | uintptr_t        | Unsigned   | 4 or 8 bytes depending on register width
String           | str, string      |            | JS strings are converted to and from UTF-8
String           | str16, string16  |            | JS strings are converted to and from UTF-16 (LE)

Primitive types can be specified by name (in a string) or through `koffi.types`:

```js
// These two lines do the same:
let struct1 = koffi.struct({ dummy: 'long' });
let struct2 = koffi.struct({ dummy: koffi.types.long });
```

### Endian-sensitive types

*New in Koffi 2.1*

Koffi defines a bunch of endian-sensitive types, which can be used when dealing with binary data (network payloads, binary file formats, etc.).

JS type          | C type                 | Bytes | Signedness | Endianness
---------------- | ---------------------- | ----- | ---------- | -------------
Number (integer) | int16_le, int16_le_t   | 2     | Signed     | Little Endian
Number (integer) | int16_be, int16_be_t   | 2     | Signed     | Big Endian
Number (integer) | uint16_le, uint16_le_t | 2     | Unsigned   | Little Endian
Number (integer) | uint16_be, uint16_be_t | 2     | Unsigned   | Big Endian
Number (integer) | int32_le, int32_le_t   | 4     | Signed     | Little Endian
Number (integer) | int32_be, int32_be_t   | 4     | Signed     | Big Endian
Number (integer) | uint32_le, uint32_le_t | 4     | Unsigned   | Little Endian
Number (integer) | uint32_be, uint32_be_t | 4     | Unsigned   | Big Endian
Number (integer) | int64_le, int64_le_t   | 8     | Signed     | Little Endian
Number (integer) | int64_be, int64_be_t   | 8     | Signed     | Big Endian
Number (integer) | uint64_le, uint64_le_t | 8     | Unsigned   | Little Endian
Number (integer) | uint64_be, uint64_be_t | 8     | Unsigned   | Big Endian

## Struct types

### Struct definition

Koffi converts JS objects to C structs, and vice-versa.

Unlike function declarations, as of now there is only one way to create a struct type, with the `koffi.struct()` function. This function takes two arguments: the first one is the name of the type, and the second one is an object containing the struct member names and types. You can omit the first argument to declare an anonymous struct.

The following example illustrates how to declare the same struct in C and in JS with Koffi:

```c
typedef struct A {
    int a;
    char b;
    const char *c;
    struct {
        double d1;
        double d2;
    } d;
} A;
```

```js
const A = koffi.struct('A', {
    a: 'int',
    b: 'char',
    c: 'const char *', // Koffi does not care about const, it is ignored
    d: koffi.struct({
        d1: 'double',
        d2: 'double'
    })
});
```

Koffi automatically follows the platform C ABI regarding alignment and padding. However, you can override these rules if needed with:

- Pack all members without padding with `koffi.pack()` (instead of `koffi.struct()`)
- Change alignment of a specific member as shown below

```js
// This struct is 3 bytes long
const PackedStruct = koffi.pack('PackedStruct', {
    a: 'int8_t',
    b: 'int16_t'
});

// This one is 10 bytes long, the second member has an alignment requirement of 8 bytes
const BigStruct = koffi.struct('BigStruct', {
    a: 'int8_t',
    b: [8, 'int16_t']
})
```

Once a struct is declared, you can use it by name (with a string, like you can do for primitive types) or through the value returned by the call to `koffi.struct()`. Only the latter is possible when declaring an anonymous struct.

```js
// The following two function declarations are equivalent, and declare a function taking an A value and returning A
const Function1 = lib.func('A Function(A value)');
const Function2 = lib.func('Function', A, [A]);
```

### Opaque types

Many C libraries use some kind of object-oriented API, with a pair of functions dedicated to create and delete objects. An obvious example of this can be found in stdio.h, with the opaque `FILE *` pointer. You can open and close files with `fopen()` and `fclose()`, and manipule the opaque pointer with other functions such as `fread()` or `ftell()`.

In Koffi, you can manage this with opaque types. Declare the opaque type with `koffi.opaque(name)`, and use a pointer to this type either as a return type or some kind of [output parameter](parameters.md#output-parameters) (with a double pointer).

```{note}
Opaque types **have changed in version 2.0, and again in version 2.1**.

In Koffi 1.x, opaque handles were defined in a way that made them usable directly as parameter and return types, obscuring the underlying pointer.

Now, you must use them through a pointer, and use an array for output parameters. This is shown in the example below (look for the call to `ConcatNewOut` in the JS part), and is described in the section on [output parameters](parameters.md#output-parameters).

In addition to this, you should use `koffi.opaque()` (introduced in Koffi 2.1) instead of `koffi.handle()` which is deprecated, and will be removed eventually in Koffi 3.0.

Consult the [migration guide](migration.md) for more information.
```

The full example below implements an iterative string builder (concatenator) in C, and uses it from Javascript to output a mix of Hello World and FizzBuzz. The builder is hidden behind an opaque type, and is created and destroyed using a pair of C functions: `ConcatNew` (or `ConcatNewOut`) and `ConcatFree`.

```c
// Build with: clang -fPIC -o handles.so -shared handles.c -Wall -O2

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef struct Fragment {
    struct Fragment *next;

    size_t len;
    char str[];
} Fragment;

typedef struct Concat {
    Fragment *first;
    Fragment *last;

    size_t total;
} Concat;

bool ConcatNewOut(Concat **out)
{
    Concat *c = malloc(sizeof(*c));
    if (!c) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
        return false;
    }

    c->first = NULL;
    c->last = NULL;
    c->total = 0;

    *out = c;
    return true;
}

Concat *ConcatNew()
{
    Concat *c = NULL;
    ConcatNewOut(&c);
    return c;
}

void ConcatFree(Concat *c)
{
    if (!c)
        return;

    Fragment *f = c->first;

    while (f) {
        Fragment *next = f->next;
        free(f);
        f = next;
    }

    free(c);
}

bool ConcatAppend(Concat *c, const char *frag)
{
    size_t len = strlen(frag);

    Fragment *f = malloc(sizeof(*f) + len + 1);
    if (!f) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
        return false;
    }

    f->next = NULL;
    if (c->last) {
        c->last->next = f;
    } else {
        c->first = f;
    }
    c->last = f;
    c->total += len;

    f->len = len;
    memcpy(f->str, frag, len);
    f->str[len] = 0;

    return true;
}

const char *ConcatBuild(Concat *c)
{
    Fragment *r = malloc(sizeof(*r) + c->total + 1);
    if (!r) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
        return NULL;
    }

    r->next = NULL;
    r->len = 0;

    Fragment *f = c->first;

    while (f) {
        Fragment *next = f->next;

        memcpy(r->str + r->len, f->str, f->len);
        r->len += f->len;

        free(f);
        f = next;
    }
    r->str[r->len] = 0;

    c->first = r;
    c->last = r;

    return r->str;
}
```

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('./handles.so');

const Concat = koffi.opaque('Concat');
const ConcatNewOut = lib.func('bool ConcatNewOut(_Out_ Concat **out)');
const ConcatNew = lib.func('Concat *ConcatNew()');
const ConcatFree = lib.func('void ConcatFree(Concat *c)');
const ConcatAppend = lib.func('bool ConcatAppend(Concat *c, const char *frag)');
const ConcatBuild = lib.func('const char *ConcatBuild(Concat *c)');

let c = ConcatNew();
if (!c) {
    // This is stupid, it does the same, but try both versions (return value, output parameter)
    let ptr = [null];
    if (!ConcatNewOut(ptr))
        throw new Error('Allocation failure');
    c = ptr[0];
}

try {
    if (!ConcatAppend(c, 'Hello... '))
        throw new Error('Allocation failure');
    if (!ConcatAppend(c, 'World!\n'))
        throw new Error('Allocation failure');

    for (let i = 1; i <= 30; i++) {
        let frag;
        if (i % 15 == 0) {
            frag = 'FizzBuzz';
        } else if (i % 5 == 0) {
            frag = 'Buzz';
        } else if (i % 3 == 0) {
            frag = 'Fizz';
        } else {
            frag = String(i);
        }

        if (!ConcatAppend(c, frag))
            throw new Error('Allocation failure');
        if (!ConcatAppend(c, ' '))
            throw new Error('Allocation failure');
    }

    let str = ConcatBuild(c);
    if (str == null)
        throw new Error('Allocation failure');
    console.log(str);
} finally {
    ConcatFree(c);
}
```

## Array types

### Fixed-size C arrays

Fixed-size arrays are declared with `koffi.array(type, length)`. Just like in C, they cannot be passed as functions parameters (they degenerate to pointers), or returned by value. You can however embed them in struct types.

Koffi applies the following conversion rules when passing arrays to/from C:

- **JS to C**: Koffi can take a normal Array (e.g. `[1, 2]`) or a TypedArray of the correct type (e.g. `Uint8Array` for an array of `uint8_t` numbers)
- **C to JS** (return value, output parameters, callbacks): Koffi will use a TypedArray if possible. But you can change this behavior when you create the array type with the optional hint argument: `koffi.array('uint8_t', 64, 'Array')`. For non-number types, such as arrays of strings or structs, Koffi creates normal arrays.

See the example below:

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

// Those two structs are exactly the same, only the array conversion hint is different
const Foo1 = koffi.struct('Foo', {
    i: 'int',
    a16: koffi.array('int16_t', 8)
});
const Foo2 = koffi.struct('Foo', {
    i: 'int',
    a16: koffi.array('int16_t', 8, 'Array')
});

// Uses an hypothetical C function that just returns the struct passed as a parameter
const ReturnFoo1 = lib.func('Foo1 ReturnFoo(Foo1 p)');
const ReturnFoo2 = lib.func('Foo2 ReturnFoo(Foo2 p)');

console.log(ReturnFoo1({ i: 5, a16: [6, 8] })) // Prints { i: 5, a16: Int16Array(2) [6, 8] }
console.log(ReturnFoo2({ i: 5, a16: [6, 8] })) // Prints { i: 5, a16: [6, 8] }
```

### Fixed-size string buffers

Koffi can also convert JS strings to fixed-sized arrays in the following cases:

- **char arrays** are filled with the UTF-8 encoded string, truncated if needed. The buffer is always NUL-terminated.
- **char16 (or char16_t) arrays** are filled with the UTF-16 encoded string, truncated if needed. The buffer is always NUL-terminated.

The reverse case is also true, Koffi can convert a C fixed-size buffer to a JS string. This happens by default for char, char16 and char16_t arrays, but you can also explicitly ask for this with the `String` array hint (e.g. `koffi.array('char', 8, 'String')`).

### Array pointers (dynamic arrays)

In C, dynamically-sized arrays are usually passed around as pointers. Read more about [array pointers](pointers.md#array-pointers-dynamic-arrays) in the relevant section.
