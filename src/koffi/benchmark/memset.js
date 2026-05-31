#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/memset_napi.node');
const koffi = require('..');
const koffi2 = optional('koffi');
const ctypes = optional('node-ctypes');
const ffi_rs = optional('ffi-rs');
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
        'Node-API': time => runNapi(time),
        'Koffi v3': time => runKoffi(koffi, time),
        'Koffi v2': time => koffi2 ? runKoffi(koffi2, time) : undefined,
        'node-ctypes': ctypes ? time => runNodeCTypes(time) : undefined,
        'ffi-rs': ffi_rs ? time => runFfiRs(time) : undefined,
        'node:ffi': ffi ? time => runNodeFfi(time) : undefined,
        'ffi-napi': ffi_napi ? time => runFfiNapi(time) : undefined
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

function runKoffi(koffi, time) {
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

function runNodeCTypes(time) {
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

function runFfiRs(time) {
    ffi_rs.open({
        library: 'libc',
        path: process.platform == 'win32' ? 'msvcrt.dll' : ''
    });

    // I could not figure out how to simply pass a Buffer to a function that expects a pointer,
    // or how node-ffi-rs pointers are supposed to work. I did not try for long.
    // Instead, just use BigInt for the pointers, and use koffi.address() to get the address
    // from the buffer.
    const lib = ffi_rs.define({
        memset: {
            library: 'libc',
            retType: ffi_rs.DataType.BigInt,
            paramsType: [ffi_rs.DataType.BigInt, ffi_rs.DataType.I32, ffi_rs.DataType.U64]
        }
    });

    let buf = Buffer.alloc(8192);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            lib.memset([koffi.address(buf), 42, buf.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runNodeFfi(time) {
    const lib = new ffi.DynamicLibrary(process.platform == 'win32' ? 'msvcrt.dll' : null);

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

function runFfiNapi(time) {
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
    if (process.isBun && mod.startsWith('@napi-ffi/'))
        return null;

    try {
        return require(mod);
    } catch (err) {
        return null;
    }
}
