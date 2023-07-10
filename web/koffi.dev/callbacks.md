# Callbacks

*Changed in Koffi 2.4*

```{note}
The function `koffi.proto()` was introduced in Koffi 2.4, it was called `koffi.callback()` in earlier versions.
```

## Callback types

In order to pass a JS function to a C function expecting a callback, you must first create a callback type with the expected return type and parameters. The syntax is similar to the one used to load functions from a shared library.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

// With the classic syntax, this callback expects an integer and returns nothing
const ExampleCallback = koffi.proto('ExampleCallback', 'void', ['int']);

// With the prototype parser, this callback expects a double and float, and returns the sum as a double
const AddDoubleFloat = koffi.proto('double AddDoubleFloat(double d, float f)');
```

Once your callback type is declared, you can use a pointer to it in struct definitions, as function parameters and/or return types, or to call/decode function pointers.

```{note}
Callbacks **have changed in version 2.0**.

In Koffi 1.x, callbacks were defined in a way that made them usable directly as parameter and return types, obscuring the underlying pointer.

Now, you must use them through a pointer: `void CallIt(CallbackType func)` in Koffi 1.x becomes `void CallIt(CallbackType *func)` in version 2.0 and newer.

Consult the [migration guide](migration.md) for more information.
```

## Transient and registered callbacks

Koffi only uses predefined static trampolines, and does not need to generate code at runtime, which makes it compatible with platforms with hardened W^X migitations (such as PaX mprotect). However, this imposes some restrictions on the maximum number of callbacks, and their duration.

Thus, Koffi distinguishes two callback modes:

- [Transient callbacks](#transient-callbacks) can only be called while the C function they are passed to is running, and are invalidated when it returns. If the C function calls the callback later, the behavior is undefined, though Koffi tries to detect such cases. If it does, an exception will be thrown, but this is no guaranteed. However, they are simple to use, and don't require any special handling.
- [Registered callbacks](#registered-callbacks) can be called at any time, but they must be manually registered and unregistered. A limited number of registered callbacks can exist at the same time.

You need to specify the correct [calling convention](functions.md#calling-conventions) on x86 platforms, or the behavior is undefined (Node will probably crash). Only *cdecl* and *stdcall* callbacks are supported.

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
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('./callbacks.so'); // Fake path

const TransferCallback = koffi.proto('int TransferCallback(const char *str, int age)');

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

*New in Koffi 2.0 (explicit this binding in Koffi 2.2)*

Use registered callbacks when the function needs to be called at a later time (e.g. log handler, event handler, `fopencookie/funopen`). Call `koffi.register(func, type)` to register a callback function, with two arguments: the JS function, and the callback type.

When you are done, call `koffi.unregister()` (with the value returned by `koffi.register()`) to release the slot. A maximum of 1024 callbacks can exist at the same time. Failure to do so will leak the slot, and subsequent registrations may fail (with an exception) once all slots are used.

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
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('./callbacks.so'); // Fake path

const GetCallback = koffi.proto('const char *GetCallback(const char *name)');
const PrintCallback = koffi.proto('void PrintCallback(const char *str)');

const RegisterFunctions = lib.func('void RegisterFunctions(GetCallback *cb1, PrintCallback *cb2)');
const SayIt = lib.func('void SayIt(const char *name)');

let cb1 = koffi.register(name => 'Hello ' + name + '!', koffi.pointer(GetCallback));
let cb2 = koffi.register(console.log, 'PrintCallback *');

RegisterFunctions(cb1, cb2);
SayIt('Kyoto'); // Prints Hello Kyoto!

koffi.unregister(cb1);
koffi.unregister(cb2);
```

Starting *with Koffi 2.2*, you can optionally specify the `this` value for the function as the first argument.

```js
class ValueStore {
    constructor(value) { this.value = value; }
    get() { return this.value; }
}

let store = new ValueStore(42);

let cb1 = koffi.register(store.get, 'IntCallback *'); // If a C function calls cb1 it will fail because this will be undefined
let cb2 = koffi.register(store, store.get, 'IntCallback *'); // However in this case, this will match the store object
```

## Special considerations

### Decoding pointer arguments

*New in Koffi 2.2, changed in Koffi 2.3*

Koffi does not have enough information to convert callback pointer arguments to an appropriate JS value. In this case, your JS function will receive an opaque *External* object.

You can pass this value through to another C function that expects a pointer of the same type, or you can use `koffi.decode()` to decode it into something you can use in Javascript.

Some arguments are optional and this function can be called in several ways:

- `koffi.decode(value, type)`: no offset, expect NUL-terminated strings
- `koffi.decode(value, offset, type)`: explicit offset to add to the pointer before decoding

The following example sorts an array of strings (in-place) with `qsort()`:

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('libc.so.6');

const SortCallback = koffi.proto('int SortCallback(const void *first, const void *second)');
const qsort = lib.func('void qsort(_Inout_ void *array, size_t count, size_t size, SortCallback *cb)');

let array = ['foo', 'bar', '123', 'foobar'];

qsort(koffi.as(array, 'char **'), array.length, koffi.sizeof('void *'), (ptr1, ptr2) => {
    let str1 = koffi.decode(ptr1, 'char *');
    let str2 = koffi.decode(ptr2, 'char *');

    return str1.localeCompare(str2);
});

console.log(array); // Prints ['123', 'bar', 'foo', 'foobar']
```

There is also an optional ending `length` argument that you can use in two cases:

- Use it to give the number of bytes to decode in non-NUL terminated strings: `koffi.decode(value, 'char *', 5)`
- Decode consecutive values into an array. For example, here is how you can decode an array with 3 float values: `koffi.decode(value, 'float', 3)`. This is equivalent to `koffi.decode(value, koffi.array('float', 3))`.

```{note}
In Koffi 2.2 and earlier versions, the length argument is only used to decode strings and is ignored otherwise.
```

### Asynchronous callbacks

*New in Koffi 2.2.2*

JS execution is inherently single-threaded, so JS callbacks must run on the main thread. There are two ways you may want to call a callback function from another thread:

- Call the callback from an asynchronous FFI call (e.g. `waitpid.async`)
- Inside a synchronous FFI call, pass the callback to another thread

In both cases, Koffi will queue the call back to JS to run on the main thread, as soon as the JS event loop has a chance to run (for example when you await a promise).

```{warning}
Be careful, you can easily get into a deadlock situation if you call a callback from a secondary thread and your main thread never lets the JS event loop run (for example, if the main thread waits for the secondary thread to finish something itself).
```

## Handling of exceptions

If an exception happens inside the JS callback, the C API will receive 0 or NULL (depending on the return value type).

Handle the exception yourself (with try/catch) if you need to handle exceptions differently.
