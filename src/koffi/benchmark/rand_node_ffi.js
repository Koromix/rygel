#!/usr/bin/env node

// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

const ref = require('ref-napi');
const ffi = require('ffi-napi');
const struct = require('ref-struct-di')(ref);
const { performance } = require('perf_hooks');

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
        srand: ['void', ['uint']],
        rand: ['int', []]
    });

    lib.srand(42);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            sum += lib.rand();

        iterations += 1000000;
    }

    time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}

