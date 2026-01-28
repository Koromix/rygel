#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/atoi_napi.node');
const koffi = require('../../koffi');
const ctypes = require('node-ctypes');
const ref = require('@napi-ffi/ref-napi');
const ffi = require('@napi-ffi/ffi-napi');
const struct = require('@napi-ffi/ref-struct-di')(ref);
const { performance } = require('perf_hooks');

const StringTable = [
    '424242',
    'foobar',
    '123456789'
];

main();

function main() {
    let time = 3000;

    if (process.argv.length >= 3) {
        time = parseFloat(process.argv[2]) * 1000;
        if (Number.isNaN(time))
            throw new Error('Not a valid number');
        if (time < 0)
            throw new Error('Time must be positive');
    }

    let perf = {
        'napi': run_napi(time),
        'koffi': run_koffi(time),
        'node-ctypes': run_node_ctypes(time),
        'node-ffi-napi': run_node_ffi(time)
    }

    console.log(JSON.stringify(perf, null, 4));
}

function run_napi(time) {
    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            napi.atoi(StringTable[i % StringTable.length]);

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
            atoi(StringTable[i % StringTable.length]);

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
            atoi(StringTable[i % StringTable.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi(time) {
    const lib = ffi.Library(process.platform == 'win32' ? 'msvcrt.dll' : null, {
        atoi: ['int', ['string']]
    });

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            lib.atoi(StringTable[i % StringTable.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}
