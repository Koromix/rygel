#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/qsort_napi.node');
const koffi = require('..');
const koffi2 = optional('koffi2');
const ctypes = optional('node-ctypes');
const ffi = optional('node:ffi');
const { performance } = require('perf_hooks');

const TIME = 8000;

const RANDOM_INTS = Array.from({ length: 200 }, () => Math.floor(Math.random() * 1000) + 1);

main();

// This benchmark tests callbacks, not decode/conversion functions.
// So they all use koffi.decode.int() inside the callbacks.

function main() {
    let args = process.argv.slice(2);

    let tests = {
        'napi': time => runNapi(time),
        'koffi (JS array)': time => runKoffiArray(koffi, time),
        'koffi (Buffer)': time => runKoffiBuffer(koffi, time),
        'koffi2 (JS array)': time => koffi2 ? runKoffiArray(koffi2, time) : undefined,
        'koffi2 (Buffer)': time => koffi2 ? runKoffiBuffer(koffi2, time) : undefined,
        'node-ctypes': ctypes ? time => runNodeCTypes(time) : undefined,
        'node:ffi': ffi ? time => runNodeFfi(time) : undefined
    };

    let perf = Object.fromEntries(Object.keys(tests).map(key => {
        let [engine] = key.split(' ');
        let func = tests[key];

        if (func == null)
            return [key, undefined];
        if (args.length && !args.includes(engine))
            return [key, undefined];

        return [key, func(TIME)];
    }));

    console.log(JSON.stringify(perf, null, 4));
}

function runNapi(time) {
    let start = performance.now();
    let iterations = 0;

    const cmp = (ptr1, ptr2) => {
        let a = koffi.decode.int(ptr1);
        let b = koffi.decode.int(ptr2);

        return a - b;
    };

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = new Int32Array(RANDOM_INTS);
            napi.qsort(array, array.length, array.BYTES_PER_ELEMENT, cmp);
            let sorted = Array.from(array);
        }

        iterations += 10000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runKoffiArray(koffi, time) {
    koffi.reset();

    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const SortCallback = koffi.proto('int SortCallback(const void *first, const void *second)');
    const qsort = lib.func('void qsort(_Inout_ void *array, size_t count, size_t size, SortCallback *cb)');

    const cmp = (ptr1, ptr2) => {
        let a = koffi.decode.int(ptr1);
        let b = koffi.decode.int(ptr2);

        return a - b;
    };
    const callback = koffi.register(cmp, 'SortCallback *');

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = RANDOM_INTS.slice();
            let cast = koffi.as(array, 'int *');

            qsort(cast, array.length, koffi.sizeof('int'), callback);
        }

        iterations += 10000;
    }

    koffi.unregister(callback);

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runKoffiBuffer(koffi, time) {
    koffi.reset();

    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const SortCallback = koffi.proto('int SortCallback(const void *first, const void *second)');
    const qsort = lib.func('void qsort(_Inout_ void *array, size_t count, size_t size, SortCallback *cb)');

    const cmp = (ptr1, ptr2) => {
        let a = koffi.decode.int(ptr1);
        let b = koffi.decode.int(ptr2);

        return a - b;
    };
    const callback = koffi.register(cmp, 'SortCallback *');

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = new Int32Array(RANDOM_INTS);
            qsort(array, array.length, koffi.sizeof('int'), callback);
            let sorted = Array.from(array);
        }

        iterations += 10000;
    }

    koffi.unregister(callback);

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runNodeCTypes(time) {
    let lib = new ctypes.CDLL(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const IntArray = ctypes.array(ctypes.c_int32, RANDOM_INTS.length);
    const qsort = lib.func('qsort', ctypes.c_void, [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_void_p]);

    const cmp = (ptr1, ptr2) => {
        let a = koffi.decode.int(ptr1);
        let b = koffi.decode.int(ptr2);

        return a - b;
    };
    const callback = ctypes.callback(cmp, ctypes.c_int32, [ctypes.c_void_p, ctypes.c_void_p]);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = IntArray.create(RANDOM_INTS);
            qsort(ctypes.byref(array), RANDOM_INTS.length, ctypes.sizeof(ctypes.c_int32), callback.pointer);
            let sorted = Array.from({ length: RANDOM_INTS.length }, (_, i) => array[i]);
        }

        iterations += 10000;
    }

    callback.release();

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runNodeFfi(time) {
    let lib = new ffi.DynamicLibrary(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const qsort = lib.getFunction('qsort', {
        parameters: ['ptr', 'u64', 'u64', 'ptr'],
        result: 'void'
    });

    const cmp = (ptr1, ptr2) => {
        let a = koffi.decode.int(ptr1);
        let b = koffi.decode.int(ptr2);

        return a - b;
    };
    const callback = lib.registerCallback({ result: 'i32', parameters: ['ptr', 'ptr'] }, cmp);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        let items = BigInt(RANDOM_INTS.length);

        for (let i = 0; i < 10000; i++) {
            let array = new Int32Array(RANDOM_INTS);
            qsort(array, items, 4n, callback);
            let sorted = Array.from({ length: RANDOM_INTS.length }, (_, i) => array[i]);
        }

        iterations += 10000;
    }

    lib.unregisterCallback(callback);

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function optional(mod) {
    if (process.isBun && mod.startsWith('@napi-ffi/'))
        return null;

    try {
        return require(mod);
    } catch (err) {
        return null;
    }
}
