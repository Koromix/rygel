# Data types

## Primitive types

While the C standard allows for variation in the size of most integer types, Koffi enforces the same definition for most primitive types, listed below:

JS type           | C type             | Bytes | Signedness | Note
----------------- | ------------------ | ----- | ---------- | ---------------------------
Null              | void               | 0     |            | Only valid as a return type
Number (integer)  | int8               | 1     | Signed     |
Number (integer)  | int8_t             | 1     | Signed     |
Number (integer)  | uint8              | 1     | Unsigned   |
Number (integer)  | uint8_t            | 1     | Unsigned   |
Number (integer)  | char               | 1     | Signed     |
Number (integer)  | uchar              | 1     | Unsigned   |
Number (integer)  | unsigned char      | 1     | Unsigned   |
Number (integer)  | char16             | 2     | Signed     |
Number (integer)  | char16_t           | 2     | Signed     |
Number (integer)  | int16              | 2     | Signed     |
Number (integer)  | int16_t            | 2     | Signed     |
Number (integer)  | uint16             | 2     | Unsigned   |
Number (integer)  | uint16_t           | 2     | Unsigned   |
Number (integer)  | short              | 2     | Signed     |
Number (integer)  | unsigned short     | 2     | Unsigned   |
Number (integer)  | int32              | 4     | Signed     |
Number (integer)  | int32_t            | 4     | Signed     |
Number (integer)  | uint32             | 4     | Unsigned   |
Number (integer)  | uint32_t           | 4     | Unsigned   |
Number (integer)  | int                | 4     | Signed     |
Number (integer)  | uint               | 4     | Unsigned   |
Number (integer)  | unsigned int       | 4     | Unsigned   |
Number (integer)  | int64              | 8     | Signed     |
Number (integer)  | int64_t            | 8     | Signed     |
Number (integer)  | uint64             | 8     | Unsigned   |
Number (integer)  | uint64_t           | 8     | Unsigned   |
Number (integer)  | longlong           | 8     | Signed     |
Number (integer)  | long long          | 8     | Signed     |
Number (integer)  | ulonglong          | 8     | Unsigned   |
Number (integer)  | unsigned long long | 8     | Unsigned   |
Number (float)    | float32            | 4     |            |
Number (float)    | float64            | 8     |            |
Number (float)    | float              | 4     |            |
Number (float)    | double             | 8     |            |

Koffi defines a few more types that can change size depending on the OS and the architecture:

JS type          | C type        | Signedness | Note
---------------- | ------------- | ---------- | ------------------------------------------------
Boolean          | bool          |            | Usually one byte
Number (integer) | long          | Signed     | 4 or 8 bytes depending on platform (LP64, LLP64)
Number (integer) | ulong         | Unsigned   | 4 or 8 bytes depending on platform (LP64, LLP64)
Number (integer) | unsigned long | Unsigned   | 4 or 8 bytes depending on platform (LP64, LLP64)
String           | string        |            | JS strings are converted to and from UTF-8
String           | string16      |            | JS strings are converted to and from UTF-16 (LE)

Primitive types can be specified by name (in a string) or through `koffi.types`:

```js
// These two lines do the same:
let struct1 = koffi.struct({ dummy: 'int' });
let struct2 = koffi.struct({ dummy: koffi.types.long });
```

## Struct types

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
    c: 'string',
    d: koffi.struct({
        d1: 'double',
        d2: 'double'
    })
});
```

Koffi follows the C and ABI rules regarding struct alignment and padding.

Once a struct is declared, you can use it by name (with a string, like you can do for primitive types) or the through the value returned by the call to `koffi.struct()`. Only the latter is possible when declaring an anonymous struct.

```js
// The following two function declarations are equivalent, and declare a function taking an A value and returning A
const Function1 = lib.func('A Function(A value)');
const Function2 = lib.func('Function', A, [A]);
```

## Pointer types

In C, pointer arguments are used for differenty purposes. It is important to distinguish these use cases because Koffi provides different ways to deal with each of them:

- **Struct pointers**: Use of struct pointers by C libraries fall in two cases: avoid (potentially) expensive copies, and to let the function change struct contents (output or input/output argument).
- **Opaque handles**: the library does not expose the contents of the structs, and only provides you with a pointer to it (e.g. `sqlite3_stmt`). Only the functions provided by the library can do something with this pointer, in Koffi we call this a handle. This is usually done for ABI-stability reason, and to prevent library users from messing directly with library internals.
- **Arrays**: in C, you dynamically-sized arrays are usually passed to functions with pointers, either NULL-terminated or with an additional length argument.
- **Pointers to primitive types**: This is more rare, and generally used for output or input/output arguments. The Win32 API has a lot of these.

### Struct pointers

The following Win32 example uses `GetCursorPos()` (with an output parameter) to retrieve and show the current cursor position.

```js
const koffi = require('koffi');
const lib = koffi.load('kernel32.dll');

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

### Opaque handles

Many C libraries use some kind of object-oriented API, with a pair of functions dedicated to create and delete objects. An obvious example of this can be found in stdio.h, with the opaque `FILE *` pointer. You can open and close files with `fopen()` and `fclose()`, and manipule the handle with other functions such as `fread()` or `ftell()`.

In Koffi, you can manage this with opaque handles. Declare the handle type with `koffi.handle(name)`, and use this type either as a return type or some kind of [output parameter](functions.md#output-parameters) (with a pointer to the handle).

The full example below implements an iterative string builder (concatenator) in C, and uses it from Javascript to output a mix of Hello World and FizzBuzz. The builder is hidden behind an opaque handle, and is created and destroyed using a pair of C functions: `ConcatNew` (or `ConcatNewOut`) and `ConcatFree`.

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
const koffi = require('koffi');
const lib = koffi.load('./handles.so');

const Concat = koffi.handle('Concat');
const ConcatNewOut = lib.func('bool ConcatNewOut(Concat *out)');
const ConcatNew = lib.func('Concat ConcatNew()');
const ConcatFree = lib.func('void ConcatFree(Concat c)');
const ConcatAppend = lib.func('bool ConcatAppend(Concat c, const char *frag)');
const ConcatBuild = lib.func('const char *ConcatBuild(Concat c)');

let c = ConcatNew();
if (!c) {
    // This is stupid, it does the same, but try both versions (return value, output parameter)
    if (!ConcatNewOut(c))
        throw new Error('Allocation failure');
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

### Array pointers

```js
```

### Pointers to primitive types

In javascript, it is not possible to pass a primitive value by reference to another function. This means that you cannot call a function and expect it to modify the value of one of its number or string parameter.

However, arrays and objects (among others) are reference type values. Assigning an array or an object from one variable to another does not invole any copy. Instead, as the following example illustrates, the new variable references the same list as the first:

```js
let list1 = [1, 2];
let list2 = list1;

list2[1] = 42;

console.log(list1); // Prints [1, 42]
```

All of this means that C functions that are expected to modify their primitive output values (such as an `int *` parameter) cannot be used directly. However, thanks to Koffi's transparent array support, you can use Javascript arrays to approximate reference semantics with single-element arrays.

Below, you can find an example of an addition function where the result is stored in an `int *` output parameter and how to use this function from Koffi.

```c
void AddInt(int *dest, int add)
{
    *dest += add;
}
```

You can simply pass a single-element array as the third argument:

```js
const AddInt = lib.func('void AddInt(_Inout_ int *dest, int add)');

let sum = [36];
AddInt(sum, 6);

console.log(sum[0]); // Prints 42
```

## Fixed-size C arrays

Fixed-size arrays are declared with `koffi.array(type, length)`. Just like in C, they cannot be passed as functions parameters (they degenerate to pointers), or returned by value. You can however embed them in struct types.

Koffi applies the following conversion rules when passing arrays to/from C:

- **JS to C**: Koffi can take a normal Array (e.g. `[1, 2]`) or a TypedArray of the correct type (e.g. `Uint8Array` for an array of `uint8_t` numbers)
- **C to JS** (return value, output parameters, callbacks): Koffi will use a TypedArray if possible. But you can change this behavior when you create the array type with the optional hint argument: `koffi.array('uint8_t', 64, 'array')`. For non-number types, such as arrays of strings or structs, Koffi creates normal arrays.

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

### Handling of strings

Koffi can also convert JS strings to fixed-sized arrays in the following cases:

- **char arrays** are filled with the UTF-8 encoded string, truncated if needed. The buffer is always NUL-terminated.
- **char16 (or char16_t) arrays** are filled with the UTF-16 encoded string, truncated if needed. The buffer is always NUL-terminated.

The reverse case is also true, Koffi can convert a C fixed-size buffer to a JS string. This happens by default for char, char16 and char16_t arrays, but you can also explicitly ask for this with the `string` array hint (e.g. `koffi.array('char', 8, 'string')`).

## Type introspection

Koffi exposes three functions to explore type information:

- `koffi.sizeof(type)` to get the size of a type
- `koffi.alignof(type)` to get the alignment of a type
- `koffi.introspect(type)` to get the definition of a type (only for structs for now)

Just like before, you can refer to primitive types by their name or through `koffi.types`:

```js
// These two lines do the same:
console.log(koffi.sizeof('long'));
console.log(koffi.sizeof(koffi.types.long));
```
