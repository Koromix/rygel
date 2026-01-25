#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const ref = require('@napi-ffi/ref-napi');
const ffi = require('@napi-ffi/ffi-napi');
const struct = require('@napi-ffi/ref-struct-di')(ref);
const { performance } = require('perf_hooks');

const strings = [
    '424242',
    'foobar',
    '123456789'
];

let sum = 0;

main();

async function main() {
    let time = 5000;

    if (process.argv.length >= 3) {
        time = parseFloat(process.argv[2]) * 1000;
        if (Number.isNaN(time))
            throw new Error('Not a valid number');
        if (time < 0)
            throw new Error('Time must be positive');
    }

    const lib = ffi.Library(process.platform == 'win32' ? 'msvcrt.dll' : null, {
        atoi: ['int', ['string']]
    });

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            sum += lib.atoi(strings[i % strings.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}
