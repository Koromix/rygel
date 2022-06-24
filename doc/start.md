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

This is a small example for Linux systems, which uses `gettimeofday()`, `localtime_r()` and `printf()` to print the current time.

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
const time_t = koffi.struct('time_t', { value: 'int64_t' });
const tm = koffi.struct('tm', {
    tm_sec: 'int',
    tm_min: 'int',
    tm_hour: 'int',
    tm_mday: 'int',
    tm_mon: 'int',
    tm_year: 'int',
    tm_wday: 'int',
    tm_yday: 'int',
    tm_isdst: 'int'
});

// Find functions
const gettimeofday = lib.func('int gettimeofday(_Out_ timeval *tv, _Out_ timezone *tz)');
const localtime_r = lib.func('tm *localtime_r(const time_t *timeval, _Out_ tm *result)');
const printf = lib.func('int printf(const char *format, ...)');

// Get local time
let tv = {};
let now = {};
gettimeofday(tv, null);
localtime_r({ value: tv.tv_sec }, now);

// And format it with printf (variadic function)
printf('Hello World!\n');
printf('Local time: %02d:%02d:%02d\n', 'int', now.tm_hour, 'int', now.tm_min, 'int', now.tm_sec);
```

## Small Windows example

This is a small example targeting the Win32 API, using `MessageBox()` to show a *Hello World!* message to the user.

It illustrates the use of the x86 stdcall calling convention.

```js
const koffi = require('koffi');

// Load the shared library
const lib = koffi.load('user32.dll');

// Declare constants
const MB_ICONINFORMATION = 0x40;

// Find functions
const MessageBoxA = lib.stdcall('MessageBoxA', 'int', ['void *', 'string', 'string', 'uint']);

MessageBoxA(null, 'Hello World!', 'Koffi', MB_ICONINFORMATION);
```
