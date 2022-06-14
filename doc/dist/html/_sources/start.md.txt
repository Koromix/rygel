# Quick start

You can install Koffi with npm:

```sh
npm install koffi
```

Once you have installed Koffi, you can start by loading it this way:

```js
const koffi = require('koffi');
```

Below you can find three examples:

* The first one runs on Linux. The functions are declared with the C-like prototype language.
* The second one runs on Windows, and uses the node-ffi like syntax to declare functions.

## Small Linux example

This is a small example for Linux systems, which uses `gettimeofday()` and `printf()` to print the current time and the timezone.

It illustrates the use of output parameters.

```js
const koffi = require('koffi');

// Load the shared library
const lib = koffi.load('libc.so.6');

// Declare struct types
const timeval = koffi.struct('timeval', {
    tv_sec: 'unsigned int',
    tv_usec: 'unsigned int'
});
const timezone = koffi.struct('timezone', {
    tz_minuteswest: 'int',
    tz_dsttime: 'int'
});

// Find functions
const gettimeofday = lib.func('int gettimeofday(_Out_ timeval *tv, _Out_ timezone *tz)');
const printf = lib.func('int printf(const char *format, ...)');

let tv = {};
let tz = {};
gettimeofday(tv, tz);

printf('Hello World!, it is: %d\n', 'int', tv.tv_sec);
console.log(tz);
```

## Small Windows example

This is a small example targeting the Win32 API, using `MessageBox()` to show a Hello message to the user.

It illustrates the use of the x86 stdcall calling convention.

```js
const koffi = require('koffi');

// Load the shared library
const lib = koffi.load('user32.dll');

// Declare constants
const MB_ICONINFORMATION = 0x40;

// Find functions
const MessageBoxA = lib.stdcall('MessageBoxA', 'int', ['void *', 'string', 'string', 'uint']);

MessageBoxA(null, 'Hello', 'Foobar', MB_ICONINFORMATION);
```
