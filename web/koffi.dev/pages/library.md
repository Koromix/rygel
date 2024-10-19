# Load library

To import functions and variables, start by loading the shared library with `koffi.load(filename)`.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('/path/to/shared/library'); // File extension depends on platforms: .so, .dll, .dylib, etc.
```

The `load` function can take an optional object argument, with the following options:

```js
const options = {
    lazy: true, // Use RTLD_LAZY (lazy-binding) on POSIX platforms (by default, use RTLD_NOW)
    global: true, // Use RTLD_GLOBAL on POSIX platforms (by default, use RTLD_LOCAL)
    deep: true // Use RTLD_DEEPBIND if supported (Linux, FreeBSD)
};

const lib = koffi.load('/path/to/shared/library.so', options);
```

You can explicitly unload a library by calling `lib.unload()`. Any attempt to find or call a function from this library after unloading it will crash.

> [!NOTE]
> On some platforms (such as with the [musl C library on Linux](https://wiki.musl-libc.org/functional-differences-from-glibc.html#Unloading-libraries)), shared libraries cannot be unloaded, so the library will remain loaded and memory mapped after the call to `lib.unload()`.

# Import functions

Use the object returned by `koffi.load()` to load C functions from the library. To do so, you can use two syntaxes:

- The classic syntax, inspired by node-ffi
- C-like prototypes

## Classic syntax

To declare a function, you need to specify its non-mangled name, its return type, and its parameters. Use an ellipsis as the last parameter for variadic functions.

```js
const printf = lib.func('printf', 'int', ['str', '...']);
const atoi = lib.func('atoi', 'int', ['str']);
```

Koffi automatically tries mangled names for non-standard x86 calling conventions. See the section on [calling conventions](functions#calling-conventions) for more information on this subject.

## C-like prototypes

If you prefer, you can declare functions using simple C-like prototype strings, as shown below:

```js
const printf = lib.func('int printf(const char *fmt, ...)');
const atoi = lib.func('int atoi(str)'); // The parameter name is not used by Koffi, and optional
```

You can use `()` or `(void)` for functions that take no argument.

# Import variables

To find an exported symbol and declare a variable, use `lib.symbol(name, type)`. You need to specify its name and its type.

```c
int my_int = 42;
const char *my_string = NULL;
```

```js
const my_int = lib.symbol('my_int', 'int');
const my_string = lib.symbol('my_string', 'const char *');
```

You cannot directly manipulate these pointers, use:

- [koffi.decode()](memory#decode-to-js-values) to read their value
- [koffi.encode()](memory#encode-to-c-memory) to change their value
