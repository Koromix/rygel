# Types

## Definition

The type object returned after a new type has been made (struct, pointer, etc.) can be introspected with various information about the type: name, primitive, size, alignment, members (record types), reference type (array, pointer) and length (array), signature (prototypes).

```js
const FoobarType = koffi.struct('FoobarType', {
    a: 'int',
    b: 'char *',
    c: 'double'
});

console.log(FoobarType);

// Expected result on 64-bit machines:
// Type {
//     name: 'FoobarType',
//     primitive: 'Record',
//     size: 24,
//     alignment: 8,
//     members: {
//         a: { name: 'a', type: [Type], offset: 0 },
//         b: { name: 'b', type: [Type], offset: 8 },
//         c: { name: 'c', type: [Type], offset: 16 }
//     }
// }
```

Koffi also exposes a few more utility functions to get a subset of this information:

- `koffi.sizeof(type)` to get the size of a type
- `koffi.alignof(type)` to get the alignment of a type
- `koffi.offsetof(type, member_name)` to get the offset of a record member
- `koffi.type(type)` to get the resolved type object from a type string

> [!NOTE]
> The function `koffi.type()` was introduced in Koffi 3.0 to replace `koffi.resolve()`, which does not exist anymore.

Just like before, you can refer to primitive types by their name or through `koffi.types`:

```js
// These three lines do the same:
console.log(koffi.sizeof('long'));
console.log(koffi.sizeof(koffi.types.long));
console.log(koffi.types.long.size);
```

## Aliases

You can alias a type with `koffi.alias(name, type)`. Aliased types are completely equivalent.

## Circular references

In some cases, composite types can point to each other and thus depend on each other. This can also happen when a function takes a pointer to a struct that also contains a function pointer.

To deal with this, you can create an opaque type and redefine it later to a concrete struct or union type, as shown below.

```js
const Type1 = koffi.opaque('Type1');

const Type2 = koffi.struct('Type2', {
    ptr: 'Type1 *',
    i: 'int'
});

// Redefine Type1 to a concrete type
koffi.struct(Type1, {
    ptr: 'Type2 *',
    f: 'float'
});
```

> [!NOTE]
> You must use a proper type object when you redefine the type. If you only have the name, use `koffi.resolve()` to get a type object from a type string.
>
> ```js
> const MyType = koffi.opaque('MyType');
>
> // This does not work, you must use the MyType object and not a type string
> koffi.struct('MyType', {
>      ptr: 'Type2 *',
>      f: 'float'
> });

# Settings

## Memory usage

For synchronous/normal calls, Koffi uses two preallocated memory blocks:

- One to construct the C stack and assign registers, subsequently used by the platform-specific assembly code (1 MiB by default)
- One to allocate strings and objects/structs (2 MiB by default)

Unless very big strings or objects (at least more than one page of memory) are used, Koffi does not directly allocate any extra memory during calls or callbacks. However, please note that the JS engine (V8) might.

The size (in bytes) of these preallocated blocks can be changed. Use `koffi.config()` to get an object with the settings, and `koffi.config(obj)` to apply new settings.

```js
let config = koffi.config();
console.log(config);
```

The same is true for asynchronous calls. When an asynchronous call is made, Koffi will allocate new blocks unless there is an unused (resident) set of blocks still available. Once the asynchronous call is finished, these blocks are freed if there are more than `resident_async_pools` sets of blocks left around.

There cannot be more than `max_async_calls` running at the same time.

## Default settings

Setting              | Default | Description
-------------------- | ------- | -----------------------------------------------
sync_stack_size      | 1 MiB   | Stack size for synchronous calls
sync_heap_size       | 2 MiB   | Heap size for synchronous calls
async_stack_size     | 256 kiB | Stack size for asynchronous calls
async_heap_size      | 512 kiB | Heap size for asynchronous calls
resident_async_pools | 2       | Number of resident pools for asynchronous calls
max_async_calls      | 64      | Maximum number of ongoing asynchronous calls
max_type_size        | 64 MiB  | Maximum size of Koffi types (for arrays and structs)

# Usage statistics

You can use `koffi.stats()` to get a few statistics related to Koffi.

# POSIX error codes

You can use `koffi.errno()` to get the current errno value, and `koffi.errno(value)` to change it.

The standard POSIX error codes are available in `koffi.os.errno`, as shown below:

```js
const assert = require('assert');

// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

const lib = koffi.load('libc.so.6');

const close = lib.func('int close(int fd)');

close(-1);
assert.equal(koffi.errno(), koffi.os.errno.EBADF);

console.log('close() with invalid FD is POSIX compliant!');
```

# Reset internal state

You can use `koffi.reset()` to clear some Koffi internal state such as:

- Parser type names
- Asynchronous function broker (useful to avoid false positive with `jest --detectOpenHandles`)

This function is mainly intended for test code, when you execute the same code over and over and you need to reuse type names.

> [!WARNING]
> Trying to use a function or a type that was initially defined before the reset is undefined behavior and will likely lead to a crash!
