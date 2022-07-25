# Functions

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

Koffi automatically tries mangled names for non-standard x86 calling conventions. See the section on [calling conventions](#calling-conventions) for more information on this subject.

### C-like prototypes

If you prefer, you can declare functions using simple C-like prototype strings, as shown below:

```js
const printf = lib.func('int printf(const char *fmt, ...)');
const atoi = lib.func('int atoi(str)'); // The parameter name is not used by Koffi, and optional
```

You can use `()` or `(void)` for functions that take no argument.

## Function calls

### Calling conventions

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

// The following two declarations are equivalent, and use stdcall on x86 (and the default ABI on other platforms)
const MessageBoxA_1 = lib.stdcall('MessageBoxA', 'int', ['void *', 'str', 'str', 'uint']);
const MessageBoxA_2 = lib.func('int __stdcall MessageBoxA(void *hwnd, str text, str caption, uint type)');
```

### Asynchronous calls

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

### Variadic functions

Variadic functions are declared with an ellipsis as the last argument.

In order to call a variadic function, you must provide two Javascript arguments for each additional C parameter, the first one is the expected type and the second one is the value.

```js
const printf = lib.func('printf', 'int', ['str', '...']);

// The variadic arguments are: 6 (int), 8.5 (double), 'THE END' (const char *)
printf('Integer %d, double %g, str %s', 'int', 6, 'double', 8.5, 'str', 'THE END');
```

On x86 platforms, only the Cdecl convention can be used for variadic functions.

## C to JS conversion gotchas

### Output parameters

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

#### Struct example

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

#### Opaque handle example

This example opens an in-memory SQLite database, and uses the node-ffi-style function declaration syntax.

```js
const koffi = require('koffi');
const lib = koffi.load('sqlite.so');

const sqlite3_db = koffi.handle('sqlite3_db');

// Use koffi.out() on a double pointer to copy out (from C to JS) after the call
const sqlite3_open_v2 = lib.func('sqlite3_open_v2', 'int', ['str', koffi.out(koffi.pointer(sqlite3_db, 2)), 'int', 'str']);
const sqlite3_close_v2 = lib.func('sqlite3_close_v2', 'int', [koffi.pointer(sqlite3_db)]);

const SQLITE_OPEN_READWRITE = 0x2;
const SQLITE_OPEN_CREATE = 0x4;

let db = {};
if (sqlite3_open_v2(':memory:', db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, null) != 0)
    throw new Error('Failed to open database');
sqlite3_close_v2(db);
```

### Heap-allocated values

Some C functions return heap-allocated values directly or through output parameters. While Koffi automatically converts values from C to JS (to a string or an object), it does not know when something needs to be freed, or how.

For opaque handles, such as FILE, this does not matter because you will explicitly call `fclose()` on them. But some values (such as strings) get implicitly converted by Koffi, and you lose access to the original pointer. This creates a leak if the string is heap-allocated.

To avoid this, you can instruct Koffi to call a function on the original pointer once the conversion is done, by creating a disposable type with `koffi.dispose(name, type, func)`. This creates a type derived from another type, the only difference being that *func* gets called with the original pointer once the value has been converted and is not needed anymore.

The *name* can be omitted to create an anonymous disposable type. If *func* is omitted or is null, Koffi will use `koffi.free(ptr)` (which calls the standard C library *free* function under the hood).

```js
const AnonHeapStr = koffi.disposable('str'); // Anonymous type (cannot be used in function prototypes)
const NamedHeapStr = koffi.disposable('HeapStr', 'str'); // Same thing, but named so usable in function prototypes
const ExplicitFree = koffi.disposable('HeapStr16', 'str16', koffi.free); // You can specify any other JS function
```

The following example illustrates the use of a disposable type derived from *str*.

```js
const koffi = require('koffi');
const lib = koffi.load('libc.so.6');

// You can also use: const strdup = lib.func('const char *! asprintf(const char *str)')
const HeapStr = koffi.disposable('str');
const strdup = lib.cdecl('strdup', HeapStr, ['str']);

let copy = strdup('Hello!');
console.log(copy); // Prints Hello!
```

When you declare functions with the [prototype-like syntax](#c-like-prototypes), you can either use named disposables types or use the '!' shortcut qualifier with compatibles types, as shown in the example below. This qualifier creates an anonymous disposable type that calls `koffi.free(ptr)`.

```js
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

## Javascript callbacks

In order to pass a JS function to a C function expecting a callback, you must first create a callback type with the expected return type and parameters. The syntax is similar to the one used to load functions from a shared library.

```js
const koffi = require('koffi');

// With the classic syntax, this callback expects an integer and returns nothing
const ExampleCallback = koffi.callback('ExampleCallback', 'void', ['int']);

// With the prototype parser, this callback expects a double and float, and returns the sum as a double
const AddDoubleFloat = koffi.callback('double AddDoubleFloat(double d, float f)');
```

Once your callback type is declared, you can use a pointer to it in struct definitions, or as function parameters and/or return types.

```{note}
Callbacks **have changed in version 2.0**.

In Koffi 1.x, callbacks were defined in a way that made them usable directly as parameter and return types, obscuring the underlying pointer.

Now, you must use them through a pointer: `void CallIt(CallbackType func)` in Koffi 1.x becomes `void CallIt(CallbackType *func)` in version 2.0 and newer.
```

Koffi only uses predefined static trampolines, and does not need to generate code at runtime, which makes it compatible with platforms with hardened W^X migitations (such as PaX mprotect). However, this imposes some restrictions on the maximum number of callbacks, and their duration.

Thus, Koffi distinguishes two callback modes:

- [Transient callbacks](#transient-callbacks) can only be called while the C function they are passed to is running, and are invalidated when it returns. If the C function calls the callback later, the behavior is undefined, though Koffi tries to detect such cases. If it does, an exception will be thrown, but this is no guaranteed. However, they are simple to use, and don't require any special handling.
- [Registered callbacks](#registered-callbacks) can be called at any time, but they must be manually registered and unregistered. A limited number of registered callbacks can exist at the same time.

You need to specify the correct [calling convention](#calling-conventions) on x86 platforms, or the behavior is undefined (Node will probably crash). Only *cdecl* and *stdcall* callbacks are supported.

### Transient callbacks

Use transient callbacks when the native C function only needs to call them while it runs (e.g. qsort, progress callback, `sqlite3_exec`). Here is a small example with the C part and the JS part.

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
const lib = koffi.load('./callbacks.so'); // Fake path

const TransferCallback = koffi.callback('int TransferCallback(const char *str, int age)');

const TransferToJS = lib.func('TransferToJS', 'int', ['str', 'int', koffi.pointer(TransferCallback)]);

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

### Registered callbacks

Use registered callbacks when the function needs to be called at a later time (e.g. log handler, event handler, `fopencookie/funopen`). Call `koffi.register(func, type)` to register a callback function, with two arguments: the JS function, and the callback type.

When you are done, call `koffi.unregister()` (with the value returned by `koffi.register()`) to release the slot. A maximum of 16 registered callbacks can exist at the same time. Failure to do so will leak the slot, and subsequent registrations may fail (with an exception) once all slots are used.

The example below shows how to register and unregister delayed callbacks.

```c
static const char *(*g_cb1)(const char *name);
static void (*g_cb2)(const char *str);

void RegisterFunctions(const char *(*cb1)(const char *name), void (*cb2)(const char *str))
{
    g_cb1 = cb1;
    g_cb2 = cb2;
}

void SayIt(const char *name)
{
    const char *str = g_cb1(name);
    g_cb2(str);
}
```

```js
const koffi = require('koffi');
const lib = koffi.load('./callbacks.so'); // Fake path

const GetCallback = koffi.callback('const char *GetCallback(const char *name)');
const PrintCallback = koffi.callback('void PrintCallback(const char *str)');

const RegisterFunctions = lib.func('void RegisterFunctions(GetCallback *cb1, PrintCallback *cb2)');
const SayIt = lib.func('void SayIt(const char *name)');

let cb1 = koffi.register(name => 'Hello ' + name + '!', koffi.pointer(GetCallback));
let cb2 = koffi.register(console.log, 'PrintCallback *');

RegisterFunctions(cb1, cb2);
SayIt('Kyoto'); // Prints Hello Kyoto!

koffi.unregister(cb1);
koffi.unregister(cb2);
```

### Handling of exceptions

If an exception happens inside the JS callback, the C API will receive 0 or NULL (depending on the return value type).

Handle the exception yourself (with try/catch) if you need to handle exceptions differently.

## Thread safety

Asynchronous functions run on worker threads. You need to deal with thread safety issues if you share data between threads.

Callbacks must be called from the main thread, or more precisely from the same thread as the V8 intepreter. Calling a callback from another thread is undefined behavior, and will likely lead to a crash or a big mess. You've been warned!
