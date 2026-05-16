#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

main();

function main() {
    process.chdir(__dirname);

    try {
        let args = process.argv.slice(2);
        benchmark(args);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function benchmark(args) {
    let tests = [];
    let engines = [];

    for (let arg of args) {
        if (arg[0] == '-') {
            switch (arg) {
                case '--koffi': { engines.push('koffi'); } break;
                default: throw new Error(`Unknown option ${arg}`);
            }
        } else {
            tests.push(arg);
        }
    }

    if (!tests.length) {
        tests.push('rand', 'atoi', 'qsort', 'memset');

        if (process.platform != 'darwin')
            tests.push('raylib');
    }

    if (tests.includes('rand'))
        dump('rand', run('rand.js', 'N-API', engines), 'ns');
    if (tests.includes('atoi'))
        dump('atoi', run('atoi.js', 'N-API', engines), 'ns');
    if (tests.includes('qsort'))
        dump('qsort', run('qsort.js', 'N-API', engines), 'ns');
    if (tests.includes('memset'))
        dump('memset', run('memset.js', 'N-API', engines), 'ns');
    if (tests.includes('raylib'))
        dump('raylib', run('raylib.js', 'N-API', engines), 'us');
}

function run(basename, ref, engines = []) {
    if (engines.length)
        engines = [ref, ...engines];

    let filename = path.join(__dirname, basename);
    let proc = spawnSync(process.execPath, [...process.execArgv, filename, ...engines]);

    if (proc.status === null)
        throw new Error(proc.error);
    if (proc.status != 0) {
        let output = proc.stderr?.toString?.('utf-8')?.trim?.() ||
                     proc.stdout?.toString?.('utf-8')?.trim?.() ||
                     'Unknown error';
        throw new Error(output);
    }

    let results = JSON.parse(proc.stdout.toString('utf-8'));
    let tests = Object.keys(results).map(name => ({ name: name, ...results[name] }));

    if (Object.hasOwn(results, ref)) {
        let base = results[ref];

        for (let test of tests) {
            test.ratio = (base.time / base.iterations) / (test.time / test.iterations);

            if (test != base) {
                test.overhead = 1 / test.ratio - 1;
            } else {
                test.overhead = '(ref)';
            }
        }

        tests.sort((test1, test2) => test2.ratio - test1.ratio);
    } else {
        for (let test of tests) {
            test.ratio = '-';
            test.overhead = '-';
        }
    }

    return tests;
}

function dump(name, tests, unit) {
    dumpJson(name, tests);
    dumpTable(name, tests, unit);
}

function dumpJson(name, tests) {
    let results = Object.fromEntries(tests.map(test => [test.name, {
        time: test.time,
        iterations: test.iterations,
        ratio: test.ratio,
        overhead: test.overhead
    }]));

    let str = JSON.stringify(name) + ': ' + JSON.stringify(results, null, 4);

    console.log(str);
    console.log('');
}

function dumpTable(name, tests, unit) {
    let len0 = Math.max(name.length, ...tests.map(test => test.name.length));

    console.log(`${name.padEnd(len0, ' ')} | Iteration time | Relative performance | Overhead`);
    console.log(`${'-'.padEnd(len0, '-')} | -------------- | -------------------- | --------`);
    for (let test of tests) {
        let time = formatTime(test.time / test.iterations, unit);
        let ratio = (typeof test.ratio == 'number') ? 'x' + test.ratio.toFixed(test.ratio < 0.01 ? 3 : 2) : '(no ref)';
        let overhead = (typeof test.overhead == 'number') ? `${test.overhead >= 0 ? '+' : ''}${Math.round(test.overhead * 100)}%` : test.overhead;

        console.log(`${test.name.padEnd(len0, ' ')} | ${('' + time).padEnd(14, ' ')} | ${('' + ratio).padEnd(20, ' ')} | ${overhead}`);
    }

    console.log('');
}

function formatTime(time, unit) {
    switch (unit) {
        case 'ms': { return (time * 1).toFixed(1) + ' ms'; } break;
        case 'us': { return (time * 1000).toFixed(1) + ' µs'; } break;
        case 'ns': { return (time * 10000000).toFixed(0) + ' ns'; } break;
    }
}
