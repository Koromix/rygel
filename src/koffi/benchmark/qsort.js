#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/qsort_napi.node');
const koffi = require('../../koffi');
const ctypes = require('node-ctypes');
const ref = require('@napi-ffi/ref-napi');
const ffi = require('@napi-ffi/ffi-napi');
const struct = require('@napi-ffi/ref-struct-di')(ref);
const { performance } = require('perf_hooks');

const RandomArray = Array.from({ length: 200 }, () => Math.floor(Math.random() * 1000) + 1);

main();

function main() {
    let time = 8000;

    if (process.argv.length >= 3) {
        time = parseFloat(process.argv[2]) * 1000;
        if (Number.isNaN(time))
            throw new Error('Not a valid number');
        if (time < 0)
            throw new Error('Time must be positive');
    }

    let perf = {
        'napi': run_napi(time),
        'koffi (JS array)': run_koffi_array(time),
        'koffi (Buffer)': run_koffi_buffer(time),
        'node-ctypes': run_node_ctypes(time),

        // Crashes when it ends, for some reason
        // 'node-ffi-napi': run_node_ffi(time) 
    }

    console.log(JSON.stringify(perf, null, 4));
}

function run_napi(time) {
    let start = performance.now();
    let iterations = 0;

    const cmp = (ptr1, ptr2) => {
        let a = ptr1.getInt32(0, true);
        let b = ptr2.getInt32(0, true);

        return a - b;
    };

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = new Int32Array(RandomArray);
            napi.qsort(array, array.length, array.BYTES_PER_ELEMENT, cmp);
            let sorted = Array.from(array);
        }

        iterations += 10000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_koffi_array(time) {
    koffi.reset();

    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const SortCallback = koffi.proto('int SortCallback(const void *first, const void *second)');
    const qsort = lib.func('void qsort(_Inout_ void *array, size_t count, size_t size, SortCallback *cb)');

    const cmp = (ptr1, ptr2) => {
        let a = koffi.decode(ptr1, 'int');
        let b = koffi.decode(ptr2, 'int');

        return a - b;
    };
    const callback = koffi.register(cmp, 'SortCallback *');

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = RandomArray.slice();
            let cast = koffi.as(array, 'int *');

            qsort(cast, array.length, koffi.sizeof('int'), cmp);
        }

        iterations += 10000;
    }

    koffi.unregister(callback);

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_koffi_buffer(time) {
    koffi.reset();

    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const SortCallback = koffi.proto('int SortCallback(const void *first, const void *second)');
    const qsort = lib.func('void qsort(_Inout_ void *array, size_t count, size_t size, SortCallback *cb)');

    const cmp = (ptr1, ptr2) => {
        let a = koffi.decode(ptr1, 'int');
        let b = koffi.decode(ptr2, 'int');

        return a - b;
    };
    const callback = koffi.register(cmp, 'SortCallback *');

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = new Int32Array(RandomArray);
            qsort(array, array.length, koffi.sizeof('int'), cmp);
            let sorted = Array.from(array);
        }

        iterations += 10000;
    }

    koffi.unregister(callback);

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ctypes(time) {
    let lib = new ctypes.CDLL(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const IntArray = ctypes.array(ctypes.c_int32, RandomArray.length);
    const qsort = lib.func('qsort', ctypes.c_void, [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_void_p]);

    const cmp = (ptr1, ptr2) => {
        let a = ctypes.readValue(ptr1, ctypes.c_int32);
        let b = ctypes.readValue(ptr2, ctypes.c_int32);

        return a - b;
    };
    const callback = ctypes.callback(cmp, ctypes.c_int32, [ctypes.c_void_p, ctypes.c_void_p]);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 10000; i++) {
            let array = IntArray.create(RandomArray);
            qsort(ctypes.byref(array), RandomArray.length, ctypes.sizeof(ctypes.c_int32), callback.pointer);
            let sorted = Array.from({ length: RandomArray.length }, (_, i) => array[i]);
        }

        iterations += 10000;
    }

    callback.release();

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi(time) {
    const ComparatorFunc = ffi.Function('int', [ 'void *', 'void *' ]);

    const lib = ffi.Library(process.platform == 'win32' ? 'msvcrt.dll' : null, {
        qsort: ['void', ['void *', 'size_t', 'size_t', ComparatorFunc]]
    });

    const cmp = (ptr1, ptr2) => {
        ptr1 = ptr1.reinterpret(4);
        ptr1.type = 'int';
        ptr2 = ptr2.reinterpret(4);
        ptr2.type = 'int';

        let a = ptr1.deref();
        let b = ptr2.deref();

        return a - b;
    };

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000; i++) {
            let array = new Int32Array(RandomArray);
            lib.qsort(array, array.length, array.BYTES_PER_ELEMENT, cmp);
            let sorted = Array.from(array);
        }

        iterations += 1000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}
