# Output parameters

## Output and input/output

For simplicity, and because Javascript only has value semantics for primitive types, Koffi can marshal out (or in/out) multiple types of parameters:

- [Structs](input.md#struct-types) (to/from JS objects)
- [Unions](unions.md)
- [Opaque types](input.md#opaque-types)
- String buffers

In order to change an argument from input-only to output or input/output, use the following functions:

- `koffi.out()` on a pointer, e.g. `koffi.out(koffi.pointer(timeval))` (where timeval is a struct type)
- `koffi.inout()` for dual input/output parameters

The same can be done when declaring a function with a C-like prototype string, with the MSDN-like type qualifiers:

- `_Out_` for output parameters
- `_Inout_` for dual input/output parameters

### Primitive value

This Windows example enumerate all Chrome windows along with their PID and their title. The `GetWindowThreadProcessId()` function illustrates how to get a primitive value from an output argument.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const user32 = koffi.load('user32.dll');

const DWORD = koffi.alias('DWORD', 'uint32_t');
const HANDLE = koffi.pointer(koffi.opaque('HANDLE'));
const HWND = koffi.alias('HWND', HANDLE);

const FindWindowEx = user32.func('HWND __stdcall FindWindowExW(HWND hWndParent, HWND hWndChildAfter, const char16_t *lpszClass, const char16_t *lpszWindow)');
const GetWindowThreadProcessId = user32.func('DWORD __stdcall GetWindowThreadProcessId(HWND hWnd, _Out_ DWORD *lpdwProcessId)');
const GetWindowText = user32.func('int __stdcall GetWindowTextA(HWND hWnd, _Out_ uint8_t *lpString, int nMaxCount)');

for (let hwnd = null;;) {
    hwnd = FindWindowEx(0, hwnd, 'Chrome_WidgetWin_1', null);

    if (!hwnd)
        break;

    // Get PID
    let pid;
    {
        let ptr = [null];
        let tid = GetWindowThreadProcessId(hwnd, ptr);

        if (!tid) {
            // Maybe the process ended in-between?
            continue;
        }

        pid = ptr[0];
    }

    // Get window title
    let title;
    {
        let buf = Buffer.allocUnsafe(1024);
        let length = GetWindowText(hwnd, buf, buf.length);

        if (!length) {
            // Maybe the process ended in-between?
            continue;
        }

        title = koffi.decode(buf, 'char', length);
    }

    console.log({ PID: pid, Title: title });
}
```

### Struct example

This example calls the POSIX function `gettimeofday()`, and uses the prototype-like syntax.

```js
// ES6 syntax: import koffi from 'koffi';
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

### Opaque type example

This example opens an in-memory SQLite database, and uses the node-ffi-style function declaration syntax.

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('sqlite3.so');

const sqlite3 = koffi.opaque('sqlite3');

// Use koffi.out() on a double pointer to copy out (from C to JS) after the call
const sqlite3_open_v2 = lib.func('sqlite3_open_v2', 'int', ['str', koffi.out(koffi.pointer(sqlite3, 2)), 'int', 'str']);
const sqlite3_close_v2 = lib.func('sqlite3_close_v2', 'int', [koffi.pointer(sqlite3)]);

const SQLITE_OPEN_READWRITE = 0x2;
const SQLITE_OPEN_CREATE = 0x4;

let out = [null];
if (sqlite3_open_v2(':memory:', out, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, null) != 0)
    throw new Error('Failed to open database');
let db = out[0];

sqlite3_close_v2(db);
```

### String buffer example

*New in Koffi 2.2*

This example calls a C function to concatenate two strings to a pre-allocated string buffer. Since JS strings are immutable, you must pass an array with a single string instead.

```c
void ConcatToBuffer(const char *str1, const char *str2, char *out)
{
    size_t len = 0;

    for (size_t i = 0; str1[i]; i++) {
        out[len++] = str1[i];
    }
    for (size_t i = 0; str2[i]; i++) {
        out[len++] = str2[i];
    }

    out[len] = 0;
}
```

```js
const ConcatToBuffer = lib.func('void ConcatToBuffer(const char *str1, const char *str2, _Out_ char *out)');

let str1 = 'Hello ';
let str2 = 'Friends!';

// We need to reserve space for the output buffer! Including the NUL terminator
// because ConcatToBuffer() expects so, but Koffi can convert back to a JS string
// without it (if we reserve the right size).
let out = ['\0'.repeat(str1.length + str2.length + 1)];

ConcatToBuffer(str1, str2, out);

console.log(out[0]);
```
