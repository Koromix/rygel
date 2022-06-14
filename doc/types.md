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

### Struct pointers

### Opaque handles

### Pointers to primitive types

## Fixed-size C arrays

Fixed-size arrays are declared with `koffi.array(type, length)`. Just like in C, they cannot be passed
as functions parameters (they degenerate to pointers), or returned by value. You can however embed them in struct types.

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

- **char (or int8_t) arrays** are filled with the UTF-8 encoded string, truncated if needed. The buffer is always NUL-terminated.
- **char16 (or int16_t) arrays** are filled with the UTF-16 encoded string, truncated if needed. The buffer is always NUL-terminated.

The reverse case is also true, Koffi can convert a C fixed-size buffer to a JS string. Use the `string` array hint to do this (e.g. `koffi.array('char', 8, 'string')`).

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
