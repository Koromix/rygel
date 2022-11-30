# Memory usage

## How it works

For synchronous/normal calls, Koffi uses two preallocated memory blocks:

- One to construct the C stack and assign registers, subsequently used by the platform-specific assembly code (1 MiB by default)
- One to allocate strings and big objects/structs (2 MiB by default)

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
