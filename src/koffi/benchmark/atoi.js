#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/atoi_napi.node');
const koffi = require('..');
const ctypes = require('node-ctypes');
const ffi = (() => {
    try {
        return require('node:ffi');
    } catch (err) {
        return null;
    }
})();
const { node_ffi, ref, struct } = (() => {
    if (process.platform == 'win32')
        return {};
    if (process.isBun)
        return {};

    const node_ffi = require('@napi-ffi/ffi-napi');
    const ref = require('@napi-ffi/ref-napi');
    const struct = require('@napi-ffi/ref-struct-di')(ref);

    return { node_ffi, ref, struct };
})();
const { performance } = require('perf_hooks');

const TIME = 8000;

const STRINGS = [
    '424242',
    'foobar',
    '123456789'
];

main();

function main() {
    let args = process.argv.slice(2);

    let tests = {
        'napi': time => run_napi(time),
        'koffi': time => run_koffi(time),
        'node-ctypes': time => run_node_ctypes(time),
        'node:ffi': ffi ? time => run_node_ffi(time) : undefined,
        'node-ffi-napi': node_ffi ? time => run_node_ffi_napi(time) : undefined
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
    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            napi.atoi(STRINGS[i % STRINGS.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_koffi(time) {
    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const atoi = lib.func('atoi', 'int', ['str']);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            atoi(STRINGS[i % STRINGS.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ctypes(time) {
    let lib = new ctypes.CDLL(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const atoi = lib.func('atoi', ctypes.c_int32, [ctypes.c_char_p]);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            atoi(STRINGS[i % STRINGS.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi(time) {
    const lib = new ffi.DynamicLibrary(process.platform == 'win32' ? 'msvcrt.dll' : 'libc.so.6');

    const atoi = lib.getFunction('atoi', {
        parameters: ['string'],
        result: 'i32'
    });

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            atoi(STRINGS[i % STRINGS.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi_napi(time) {
    const lib = node_ffi.Library(process.platform == 'win32' ? 'msvcrt.dll' : null, {
        atoi: ['int', ['string']]
    });

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            lib.atoi(STRINGS[i % STRINGS.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}
