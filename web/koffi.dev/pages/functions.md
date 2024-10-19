# Calling conventions

By default, calling a C function happens synchronously.

Most architectures only support one procedure call standard per process. The 32-bit x86 platform is an exception to this, and Koffi supports several x86 conventions:

 Convention   | Classic form                                  | Prototype form | Description
------------- | --------------------------------------------- | -------------- | -------------------------------------------------------------------
 **Cdecl**    | `koffi.func(name, ret, params)`               | _(default)_    | This is the default convention, and the only one on other platforms
 **Stdcall**  | `koffi.func('__stdcall', name, ret, params)`  | __stdcall      | This convention is used extensively within the Win32 API
 **Fastcall** | `koffi.func('__fastcall', name, ret, params)` | __fastcall     | Rarely used, uses ECX and EDX for first two parameters
 **Thiscall** | `koffi.func('__thiscall', name, ret, params)` | __thiscall     | Rarely used, uses ECX for first parameter

You can safely use these on non-x86 platforms, they are simply ignored.

Below you can find a small example showing how to use a non-default calling convention, with the two syntaxes:

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('user32.dll');

// The following two declarations are equivalent, and use stdcall on x86 (and the default ABI on other platforms)
const MessageBoxA_1 = lib.func('__stdcall', 'MessageBoxA', 'int', ['void *', 'str', 'str', 'uint']);
const MessageBoxA_2 = lib.func('int __stdcall MessageBoxA(void *hwnd, str text, str caption, uint type)');
```

# Call types

## Synchronous calls

Once a native function has been declared, you can simply call it as you would any other JS function.

```js
const atoi = lib.func('int atoi(const char *str)');

let value = atoi('1257');
console.log(value);
```

For [variadic functions](functions#variadic-functions), you msut specificy the type and the value for each additional argument.

```js
const printf = lib.func('printf', 'int', ['str', '...']);

// The variadic arguments are: 6 (int), 8.5 (double), 'THE END' (const char *)
printf('Integer %d, double %g, str %s', 'int', 6, 'double', 8.5, 'str', 'THE END');
```

## Asynchronous calls

You can issue asynchronous calls by calling the function through its async member. In this case, you need to provide a callback function as the last argument, with `(err, res)` parameters.

```js
// ES6 syntax: import koffi from 'koffi';
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

> [!WARNING]
> Asynchronous functions run on worker threads. You need to deal with thread safety issues if you share data between threads.
>
> Callbacks must be called from the main thread, or more precisely from the same thread as the V8 intepreter. Calling a callback from another thread is undefined behavior, and will likely lead to a crash or a big mess. You've been warned!

# Variadic functions

Variadic functions are declared with an ellipsis as the last argument.

In order to call a variadic function, you must provide two Javascript arguments for each additional C parameter, the first one is the expected type and the second one is the value.

```js
const printf = lib.func('printf', 'int', ['str', '...']);

// The variadic arguments are: 6 (int), 8.5 (double), 'THE END' (const char *)
printf('Integer %d, double %g, str %s', 'int', 6, 'double', 8.5, 'str', 'THE END');
```

On x86 platforms, only the Cdecl convention can be used for variadic functions.

# Function pointers

You can call a function pointer in two ways:

- Directly call the function pointer with `koffi.call(ptr, type, ...)`
- Decode the function pointer to an actual function with `koffi.decode(ptr, type)`

The example below shows how to call an `int (*)(int, int)` C function pointer both ways, based on the following native C library:

```c
typedef int BinaryIntFunc(int a, int b);

static int AddInt(int a, int b) { return a + b; }
static int SubstractInt(int a, int b) { return a - b; }

BinaryIntFunc *GetBinaryIntFunction(const char *type)
{
    if (!strcmp(type, "add")) {
        return AddInt;
    } else if (!strcmp(type, "substract")) {
        return SubstractInt;
    } else {
        return NULL;
    }
}
```

## Call pointer directly

Use `koffi.call(ptr, type, ...)` to call a function pointer. The first two arguments are the pointer itself and the type of the function you are trying to call (declared with `koffi.proto()` as shown below), and the remaining arguments are used for the call.

```js
// Declare function type
const BinaryIntFunc = koffi.proto('int BinaryIntFunc(int a, int b)');

const GetBinaryIntFunction = lib.func('BinaryIntFunc *GetBinaryIntFunction(const char *name)');

const add_ptr = GetBinaryIntFunction('add');
const substract_ptr = GetBinaryIntFunction('substract');

let sum = koffi.call(add_ptr, BinaryIntFunc, 4, 5);
let delta = koffi.call(substract_ptr, BinaryIntFunc, 100, 58);

console.log(sum, delta); // Prints 9 and 42
```

## Decode pointer to function

Use `koffi.decode(ptr, type)` to get back a JS function, which you can then use like any other Koffi function.

This method also allows you to perform an [asynchronous call](#asynchronous-calls) with the async member of the decoded function.

```js
// Declare function type
const BinaryIntFunc = koffi.proto('int BinaryIntFunc(int a, int b)');

const GetBinaryIntFunction = lib.func('BinaryIntFunc *GetBinaryIntFunction(const char *name)');

const add = koffi.decode(GetBinaryIntFunction('add'), BinaryIntFunc);
const substract = koffi.decode(GetBinaryIntFunction('substract'), BinaryIntFunc);

let sum = add(4, 5);
let delta = substract(100, 58);

console.log(sum, delta); // Prints 9 and 42
```

# Conversion of parameters

By default, Koffi will only forward and translate arguments from Javascript to C. However, many C functions use pointer arguments for output values, or input/output values.

Among other thing, in the the following pages you will learn more about:

- How Koffi translates [input parameters](input) to C
- How you can [define and use pointers](pointers)
- How to deal with [output parameters](output)
