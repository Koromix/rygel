# Callbacks

## Type definition

In order to pass a JS function to a C function expecting a callback, you must first create a callback type with the expected return type and parameters. The syntax is similar to the one used to load functions from a shared library.

```js
const koffi = require('koffi');

// With the classic syntax, this callback expects an integer and returns nothing
const ExampleCallback = koffi.callback('ExampleCallback', 'void', ['int']);

// With the prototype parser, this callback expects a double and float, and returns the sum as a double
const AddDoubleFloat = koffi.callback('double AddDoubleFloat(double d, float f)');
```

Once your callback type is declared, you can use them in struct definitions, or as function parameter and/or return type.

On x86 platforms, only Cdecl and Stdcall callbacks are supported.

## Example

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

## Thread safety

The callback must be called from the main thread, or more precisely from the same thread as the V8 intepreter.

Calling the callback from another thread is undefined behavior, and will likely lead to a mess.
