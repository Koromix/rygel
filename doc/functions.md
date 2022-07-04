# Function calls

## Function definitions

To declare functions, start by loading the shared library with `koffi.load(filename)`.

```js
const koffi = require('koffi');
const lib = koffi.load('/path/to/shared/library'); // File extension depends on platforms: .so, .dll, .dylib, etc.
```

You can use the returned object to load C functions from the library. To do so, you can use two syntaxes:

- The classic syntax, inspired by node-ffi
- C-like prototypes

### Classic syntax

To declare a function, you need to specify its non-mangled name, its return type, and its parameters. Use an ellipsis as the last parameter for variadic functions.

```js
const printf = lib.func('printf', 'int', ['str', '...']);
const atoi = lib.func('atoi', 'int', ['str']);
```

Koffi automatically tries mangled names for non-standard x86 calling conventions. See the section [on standard calls](#synchronous-calls) for more information on this subject.

### C-like prototypes

If you prefer, you can declare functions using simple C-like prototype strings, as shown below:

```js
const printf = lib.func('int printf(const char *fmt, ...)');
const atoi = lib.func('int atoi(str)'); // The parameter name is not used by Koffi, and optional
```

You can use `()` or `(void)` for functions that take no argument.

## Synchronous calls

By default, calling a C function happens synchronously.

Most architectures only support one procedure call standard per process. The 32-bit x86 platform is an exception to this, and Koffi supports several x86 conventions:

 Convention   | Classic form                  | Prototype form | Description
------------- | ----------------------------- | -------------- | -------------------------------------------------------------------
 **Cdecl**    | `koffi.cdecl` or `koffi.func` | _(default)_    | This is the default convention, and the only one on other platforms
 **Stdcall**  | `koffi.stdcall`               | __stdcall      | This convention is used extensively within the Win32 API
 **Fastcall** | `koffi.fastcall`              | __fastcall     | Rarely used, uses ECX and EDX for first two parameters
 **Thiscall** | `koffi.thiscall`              | __thiscall     | Rarely used, uses ECX for first parameter

You can safely use these on non-x86 platforms, they are simply ignored.

Below you can find a small example showing how to use a non-default calling convention, with the two syntaxes:

```js
const koffi = require('koffi');
const lib = koffi.load('user32.dll');

// The following two declarations are equivalent, and use Stdcall
const MessageBoxA_1 = lib.stdcall('MessageBoxA', 'int', ['void *', 'str', 'str', 'uint']);
const MessageBoxA_2 = lib.func('int __stdcall MessageBoxA(void *hwnd, str text, str caption, uint type)');
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

// This program will print:
//   Hello World!
//   Result: 1257
```

These calls are executed by worker threads. It is **your responsibility to deal with data sharing issues** in the native code that may be caused by multi-threading.

You can easily convert this callback-style async function to a promise-based version with `util.promisify()` from the Node.js standard library.

Variadic functions cannot be called asynchronously.

## Variadic functions

Variadic functions are declared with an ellipsis as the last argument.

In order to call a variadic function, you must provide two Javascript arguments for each additional C parameter, the first one is the expected type and the second one is the value.

```js
const printf = lib.func('printf', 'int', ['str', '...']);

// The variadic arguments are: 6 (int), 8.5 (double), 'THE END' (const char *)
printf('Integer %d, double %g, str %s', 'int', 6, 'double', 8.5, 'str', 'THE END');
```

On x86 platforms, only the Cdecl convention can be used for variadic functions.

## Output parameters

By default, Koffi will only forward arguments from Javascript to C. However, many C functions use pointer arguments for output values, or input/output values.

For simplicity, and because Javascript only has value semantics for primitive types, Koffi can marshal out (or in/out) two types of parameters:

- [Structs](types.md#struct-types) (to/from JS objects)
- [Opaque handles](types.md#opaque-handles)

In order to change an argument from input-only to output or input/output, use the following functions:

- `koffi.out()` on a pointer, e.g. `koffi.out(koffi.pointer(timeval))` (where timeval is a struct type)
- `koffi.inout()` for dual input/output parameters

The same can be done when declaring a function with a C-like prototype string, with the MSDN-like type qualifiers:

- `_Out_` for output parameters
- `_Inout_` for dual input/output parameters

### Struct example

This example calls the POSIX function `gettimeofday()`, and uses the prototype-like syntax.

```js
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

### Opaque handle example

This example opens an in-memory SQLite database, and uses the node-ffi-style function declaration syntax.

```js
const koffi = require('koffi');
const lib = koffi.load('sqlite.so');

const sqlite3_db = koffi.handle('sqlite3_db');

// Use koffi.out() on a pointer to copy out (from C to JS) after the call
const sqlite3_open_v2 = lib.func('sqlite3_open_v2', 'int', ['str', koffi.out(koffi.pointer(sqlite3_db)), 'int', 'str']);
const sqlite3_close_v2 = lib.func('sqlite3_close_v2', 'int', [sqlite3_db]);

const SQLITE_OPEN_READWRITE = 0x2;
const SQLITE_OPEN_CREATE = 0x4;

let db = {};
if (sqlite3_open_v2(':memory:', db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, null) != 0)
    throw new Error('Failed to open database');
sqlite3_close_v2(db);
```

## Javascript callbacks

In order to pass a JS function to a C function expecting a callback, you must first create a callback type with the expected return type and parameters. The syntax is similar to the one used to load functions from a shared library.

```js
const koffi = require('koffi');

// With the classic syntax, this callback expects an integer and returns nothing
const ExampleCallback = koffi.callback('ExampleCallback', 'void', ['int']);

// With the prototype parser, this callback expects a double and float, and returns the sum as a double
const AddDoubleFloat = koffi.callback('double AddDoubleFloat(double d, float f)');
```

Once your callback type is declared, you can use it in struct definitions, or as function parameter and/or return type.

Here is a small example with the C part and the JS part.

```c
#include <string.h>

int TransferToJS(const char *name, int age, int (*cb)(const char *str, int age))
{
    char buf[64];
    snprintf(buf, sizeof(buf), "Hello %s!", str);
    return cb(buf, age);
}
```

```js
const koffi = require('koffi');

const TransferCallback = koffi.callback('int TransferCallback(const char *str, int age)');

const TransferToJS = lib.func('int TransferToJS(const char *str, int age, TransferCallback cb)');

let ret = TransferToJS('Niels', 27, (str, age) => {
    console.log(str);
    console.log('Your age is:', age);
    return 42;
});
console.log(ret);

// This example prints:
//   Hello Niels!
//   Your age is: 27
//   42
```

On x86 platforms, only Cdecl and Stdcall callbacks are supported.

## Thread safety

Asynchronous functions run on worker threads. You need to deal with thread safety issues if you share data between threads.

Callbacks must be called from the main thread, or more precisely from the same thread as the V8 intepreter. Calling a callback from another thread is undefined behavior, and will likely lead to a crash or a big mess. You've been warned!
