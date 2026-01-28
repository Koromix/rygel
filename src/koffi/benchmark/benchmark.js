#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
        format(run('rand.js', 'napi'), 'ns');
    if (!select.length || select.includes('atoi'))
        format(run('atoi.js', 'napi'), 'ns');
    if (!select.length || select.includes('memset'))
        format(run('memset.js', 'napi'), 'ns');
    if (!select.length || select.includes('raylib'))
        format(run('raylib.js', 'napi'), 'us');
}

function run(basename, ref) {
    let filename = path.join(__dirname, basename)
    let proc = spawnSync(process.execPath, [filename]);

    if (proc.status == null)
        throw new Error(proc.error);
    if (proc.status != 0) {
        let output = proc.stderr?.toString?.('utf-8')?.trim?.() ||
                     proc.stdout?.toString?.('utf-8')?.trim?.() ||
                     'Unknown error';
        throw new Error(output);
    }

    let results = JSON.parse(proc.stdout.toString('utf-8'));
    let tests = Object.keys(results).map(name => ({ name: name, ...results[name] }));

    if (!Object.hasOwn(results, ref))
        throw new Error('Failed to find reference test');
    ref = results[ref];

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
