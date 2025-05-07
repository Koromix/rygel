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

const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');

main();

function main() {
    try {
        let select = process.argv.slice(2);
        benchmark(select);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function benchmark(select) {
    if (!select.length || select.includes('rand'))
        format(run('rand', 'rand_napi'), 'ns');
    if (!select.length || select.includes('atoi'))
        format(run('atoi', 'atoi_napi'), 'ns');
    if (!select.length || select.includes('raylib'))
        format(run('raylib', 'raylib_node_raylib'), 'us');
}

function run(name, ref) {
    let tests = [];
    {
        let entries = fs.readdirSync(__dirname);

        let re = new RegExp(name + '_[a-z0-9_]+\.js$');

        for (let entry of entries) {
            if (entry.match(re)) {
                let test = {
                    name: path.basename(entry, '.js'),
                    filename: path.join(__dirname, entry)
                };

                if (test.name == ref)
                    ref = test;
                tests.push(test);
            }
        }
    }

    if (typeof ref == 'string')
        throw new Error('Failed to find reference test');

    for (let test of tests) {
        let proc = spawnSync(process.execPath, [test.filename]);

        if (proc.status == null)
            throw new Error(proc.error);
        if (proc.status !== 0)
            throw new Error(proc.stderr || proc.stdout);

        let perf = JSON.parse(proc.stdout);

        test.iterations = perf.iterations;
        test.time = perf.time;
    }

    for (let test of tests) {
        test.ratio = (ref.time / ref.iterations) / (test.time / test.iterations);

        if (test != ref) {
            test.overhead = Math.round((1 / test.ratio - 1) * 100);
        } else {
            test.overhead = '(ref)';
        }
    }

    tests.sort((test1, test2) => test2.ratio - test1.ratio);

    return tests;
}

function format(tests, unit) {
    let len0 = tests.reduce((acc, test) => Math.max(acc, test.name.length), 0);

    console.log(`${'Benchmark'.padEnd(len0, ' ')} | Iteration time | Relative performance | Overhead`);
    console.log(`${'-'.padEnd(len0, '-')} | -------------- | -------------------- | --------`);
    for (let test of tests) {
        let time = format_time(test.time / test.iterations, unit);
        let ratio = test.ratio.toFixed(test.ratio < 0.01 ? 3 : 2);
        let overhead = (typeof test.overhead == 'number') ? `${test.overhead >= 0 ? '+' : ''}${test.overhead}%` : test.overhead;

        console.log(`${test.name.padEnd(len0, ' ')} | ${('' + time).padEnd(14, ' ')} | x${('' + ratio).padEnd(19, ' ')} | ${overhead}`);
    }

    console.log('');
}

function format_time(time, unit) {
    switch (unit) {
        case 'ms': { return (time * 1).toFixed(1) + ' ms'; } break;
        case 'us': { return (time * 1000).toFixed(1) + ' µs'; } break;
        case 'ns': { return (time * 10000000).toFixed(0) + ' ns'; } break;
    }
}
