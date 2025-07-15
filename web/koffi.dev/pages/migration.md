# Koffi 1.x to 2.x

The API was changed in 2.x in a few ways, in order to reduce some excessively "magic" behavior and reduce the syntax differences between C and the C-like prototypes.

You may need to change your code if you use:

- Callback functions
- Opaque types
- `koffi.introspect()`

## Callback type changes

In Koffi 1.x, callbacks were defined in a way that made them usable directly as parameter and return types, obscuring the underlying pointer. Now, you must use them through a pointer: `void CallIt(CallbackType func)` in Koffi 1.x becomes `void CallIt(CallbackType *func)` in version 2.0 and newer.

Given the following C code:

```c
#include <string.h>

int TransferToJS(const char *name, int age, int (*cb)(const char *str, int age))
{
    char buf[64];
    snprintf(buf, sizeof(buf), "Hello %s!", str);
    return cb(buf, age);
}
```

The two versions below illustrate the API difference between Koffi 1.x and Koffi 2.x:

```js
// Koffi 1.x

const TransferCallback = koffi.proto('int TransferCallback(const char *str, int age)');

const TransferToJS = lib.func('TransferToJS', 'int', ['str', 'int', TransferCallback]);
// Equivalent to: const TransferToJS = lib.func('int TransferToJS(str s, int x, TransferCallback cb)');

let ret = TransferToJS('Niels', 27, (str, age) => {
    console.log(str);
    console.log('Your age is:', age);
    return 42;
});
console.log(ret);
```

```js
// Koffi 2.x

const TransferCallback = koffi.proto('int TransferCallback(const char *str, int age)');

const TransferToJS = lib.func('TransferToJS', 'int', ['str', 'int', koffi.pointer(TransferCallback)]);
// Equivalent to: const TransferToJS = lib.func('int TransferToJS(str s, int x, TransferCallback *cb)');

let ret = TransferToJS('Niels', 27, (str, age) => {
    console.log(str);
    console.log('Your age is:', age);
    return 42;
});
console.log(ret);
```

Koffi 1.x only supported [transient callbacks](callbacks#javascript-callbacks), you must use Koffi 2.x for registered callbacks.

> [!NOTE]
> The function `koffi.proto()` was introduced in Koffi 2.4, it was called `koffi.callback()` in earlier versions.

## Opaque type changes

In Koffi 1.x, opaque handles were defined in a way that made them usable directly as parameter and return types, obscuring the underlying pointer. Now, in Koffi 2.0, you must use them through a pointer, and use an array for output parameters.

In addition to that, `koffi.handle()` has been deprecated in Koffi 2.1 and replaced with `koffi.opaque()`. They work the same but new code should use `koffi.opaque()`, the former one will eventually be removed in Koffi 3.0.

For functions that return opaque pointers or pass them by parameter:

```js
// Koffi 1.x

const FILE = koffi.handle('FILE');
const fopen = lib.func('fopen', 'FILE', ['str', 'str']);
const fopen = lib.func('fclose', 'int', ['FILE']);

let fp = fopen('EMPTY', 'wb');
if (!fp)
    throw new Error('Failed to open file');
fclose(fp);
```

```js
// Koffi 2.1

// If you use Koffi 2.0: const FILE = koffi.handle('FILE');
const FILE = koffi.opaque('FILE');
const fopen = lib.func('fopen', 'FILE *', ['str', 'str']);
const fopen = lib.func('fclose', 'int', ['FILE *']);

let fp = fopen('EMPTY', 'wb');
if (!fp)
    throw new Error('Failed to open file');
fclose(fp);
```

For functions that set opaque handles through output parameters (such as `sqlite3_open_v2`), you must now use a single element array as shown below:

```js
// Koffi 1.x

const sqlite3 = koffi.handle('sqlite3');

const sqlite3_open_v2 = lib.func('int sqlite3_open_v2(const char *, _Out_ sqlite3 *db, int, const char *)');
const sqlite3_close_v2 = lib.func('int sqlite3_close_v2(sqlite3 db)');

const SQLITE_OPEN_READWRITE = 0x2;
const SQLITE_OPEN_CREATE = 0x4;

let db = {};

if (sqlite3_open_v2(':memory:', db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, null) != 0)
    throw new Error('Failed to open database');

sqlite3_close_v2(db);
```

```js
// Koffi 2.1

// If you use Koffi 2.0: const sqlite3 = koffi.handle('sqlite3');
const sqlite3 = koffi.opaque('sqlite3');

const sqlite3_open_v2 = lib.func('int sqlite3_open_v2(const char *, _Out_ sqlite3 **db, int, const char *)');
const sqlite3_close_v2 = lib.func('int sqlite3_close_v2(sqlite3 *db)');

const SQLITE_OPEN_READWRITE = 0x2;
const SQLITE_OPEN_CREATE = 0x4;

let db = null;

let ptr = [null];
if (sqlite3_open_v2(':memory:', ptr, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, null) != 0)
    throw new Error('Failed to open database');
db = ptr[0];

sqlite3_close_v2(db);
```

## New koffi.introspect()

In Koffi 1.x, `koffi.introspect()` would only work with struct types, and return the object passed to `koffi.struct()` to initialize the type. Now this function works with all types.

You can still get the list of struct members:

```js
const StructType = koffi.struct('StructType', { dummy: 'int' });

// Koffi 1.x
let members = koffi.introspect(StructType);

// Koffi 2.x
let members = koffi.introspect(StructType).members;
```
