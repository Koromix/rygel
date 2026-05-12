#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/rand_napi.node');
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
        'napi': time => run_napi(time),
        'koffi': time => run_koffi(koffi, time),
        'koffi2': time => koffi2 ? run_koffi(koffi2, time) : undefined,
        'node-ctypes': time => ctypes ? run_node_ctypes(time) : undefined,
        'ffi-rs': time => ffi_rs ? run_ffi_rs(time) : undefined,
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
    napi.srand(42);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            napi.rand();

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_koffi(koffi, time) {
    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const srand = lib.func('srand', 'void', ['uint']);
    const rand = lib.func('rand', 'int', []);

    srand(42);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            rand();

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ctypes(time) {
    let lib = new ctypes.CDLL(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const srand = lib.func('srand', ctypes.c_void, [ctypes.c_uint32]);
    const rand = lib.func('rand', ctypes.c_int32, []);

    srand(42);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            rand();

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_ffi_rs(time) {
    ffi_rs.open({
        library: 'libc',
        path: process.platform == 'win32' ? 'msvcrt.dll' : ''
    });

    const lib = ffi_rs.define({
        srand: {
            library: 'libc',
            retType: ffi_rs.DataType.Void,
            paramsType: [ffi_rs.DataType.I32]
        },
        rand: {
            library: 'libc',
            retType: ffi_rs.DataType.I32,
            paramsType: []
        }
    });

    lib.srand([42]);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            lib.rand([]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi(time) {
    const lib = new ffi.DynamicLibrary(process.platform == 'win32' ? 'msvcrt.dll' : 'libc.so.6');

    const srand =  lib.getFunction('srand', {
        parameters: ['u32'],
        result: 'void'
    });
    const rand =  lib.getFunction('rand', {
        parameters: [],
        result: 'i32'
    });

    srand(42);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            rand();

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi_napi(time) {
    const lib = ffi_napi.Library(process.platform == 'win32' ? 'msvcrt.dll' : null, {
        srand: ['void', ['uint']],
        rand: ['int', []]
    });

    lib.srand(42);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            lib.rand();

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
