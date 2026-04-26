#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/memset_napi.node');
const koffi = require('..');
const ctypes = optional('node-ctypes');
const ffi = optional('node:ffi');
const ffi_napi = optional('@napi-ffi/ffi-napi');
const ref = optional('@napi-ffi/ref-napi');
const struct = optional('@napi-ffi/ref-struct-di')?.(ref);
const { performance } = require('perf_hooks');

const TIME = 8000;

main();

function main() {
    let args = process.argv.slice(2);

    let tests = {
        'napi': time => run_napi(time),
        'koffi': time => run_koffi(time, true),
        'koffi (slow pointers)': time => run_koffi(time, false),
        'node-ctypes': ctypes ? time => run_node_ctypes(time) : undefined,
        'node:ffi': ffi ? time => run_node_ffi(time) : undefined,
        'node-ffi-napi': ffi_napi ? time => run_node_ffi_napi(time) : undefined
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

function run_napi(time) {
    let buf = Buffer.alloc(8192);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            napi.memset(buf, 42, buf.length);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_koffi(time, fast_pointers) {
    koffi.reset();
    koffi.config({ fast_pointers: fast_pointers });

    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const memset = lib.func('memset', 'void *', ['void *', 'int', 'size_t']);

    let buf = Buffer.alloc(8192);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            memset(buf, 42, buf.length);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ctypes(time) {
    let lib = new ctypes.CDLL(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const memset = lib.func('memset', ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_int32, ctypes.c_size_t]);

    let buf = Buffer.alloc(8192);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            memset(buf, 42, buf.length);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi(time) {
    const lib = new ffi.DynamicLibrary(process.platform == 'win32' ? 'msvcrt.dll' : 'libc.so.6');

    const memset = lib.getFunction('memset', {
        parameters: ['ptr', 'i32', 'uint64'],
        result: 'ptr'
    });

    let buf = Buffer.alloc(8192);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            memset(buf, 42, BigInt(buf.length));

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi_napi(time) {
    const lib = ffi_napi.Library(process.platform == 'win32' ? 'msvcrt.dll' : null, {
        memset: ['void *', ['void *', 'int', 'size_t']]
    });

    let buf = Buffer.alloc(8192);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            lib.memset(buf, 42, buf.length);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function optional(mod) {
    try {
        return require(mod);
    } catch (err) {
        return null;
    }
}
