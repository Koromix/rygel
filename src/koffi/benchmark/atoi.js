#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const napi = require(pkg.cnoke.output + '/atoi_napi.node');
const koffi = require('..');
const koffi2 = optional('koffi');
const ctypes = optional('node-ctypes');
const ffi_rs = optional('ffi-rs');
const ffi = optional('node:ffi');
const ffi_napi = optional('@napi-ffi/ffi-napi');
const ref = optional('@napi-ffi/ref-napi');
const struct = optional('@napi-ffi/ref-struct-di')?.(ref);
const { performance } = require('perf_hooks');

const ITERATIONS = 8;
const ITERATION_TIME = 250;

const STRINGS = [
    '424242',
    'foobar',
    '123456789'
];

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

    let keys = Object.keys(tests).filter(key => {
        let [engine] = key.split(' ');

        if (tests[key] == null)
            return false;
        if (args.length && !args.includes(engine))
            return false;

        return true;
    });

    let perf = Object.fromEntries(keys.map(key => [key, { iterations: 0, time: 0 }]));

    for (let i = 0; i < ITERATIONS; i++) {
        for (let key of shuffle(keys)) {
            let func = tests[key];
            let result = func(ITERATION_TIME);

            // Ignore warm-up results
            if (!i)
                continue;

            perf[key].iterations += result.iterations;
            perf[key].time += result.time;
        }
    }

    console.log(JSON.stringify(perf, null, 4));
}

function runNapi(time) {
    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 100000; i++)
            napi.atoi(STRINGS[i % STRINGS.length]);

        iterations += 100000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runKoffi(koffi, time) {
    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const atoi = lib.func('atoi', 'int', ['str']);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 100000; i++)
            atoi(STRINGS[i % STRINGS.length]);

        iterations += 100000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runNodeCTypes(time) {
    let lib = new ctypes.CDLL(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const atoi = lib.func('atoi', ctypes.c_int32, [ctypes.c_char_p]);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 100000; i++)
            atoi(STRINGS[i % STRINGS.length]);

        iterations += 100000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runFfiRs(time) {
    ffi_rs.open({
        library: 'libc',
        path: process.platform == 'win32' ? 'msvcrt.dll' : ''
    });

    const lib = ffi_rs.define({
        atoi: {
            library: 'libc',
            retType: ffi_rs.DataType.I32,
            paramsType: [ffi_rs.DataType.String]
        }
    });

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 100000; i++)
            lib.atoi([STRINGS[i % STRINGS.length]]);

        iterations += 100000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runNodeFfi(time) {
    const lib = new ffi.DynamicLibrary(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const atoi = lib.getFunction('atoi', {
        arguments: ['string'],
        return: 'i32'
    });

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 100000; i++)
            atoi(STRINGS[i % STRINGS.length]);

        iterations += 100000;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runFfiNapi(time) {
    const lib = ffi_napi.Library(process.platform == 'win32' ? 'msvcrt.dll' : null, {
        atoi: ['int', ['string']]
    });

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 100000; i++)
            lib.atoi(STRINGS[i % STRINGS.length]);

        iterations += 100000;
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

function shuffle(array) {
    let copy = array.slice();
    let shuffled = [];

    while (copy.length) {
        let idx = randInt(0, copy.length);

        shuffled.push(copy[idx]);

        copy[idx] = copy[copy.length - 1];
        copy.length--;
    }

    return shuffled;
}

function randInt(min, max) {
    min = Math.ceil(min);
    max = Math.floor(max);

    let rnd = Math.floor(Math.random() * (max - min)) + min;
    return rnd;
}
