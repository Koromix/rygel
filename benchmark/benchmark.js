#!/usr/bin/env node

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const minimatch = require('minimatch');

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
        format(run('rand', 'rand_napi'));
    if (!select.length || select.includes('atoi'))
        format(run('atoi', 'atoi_napi'));
    if (!select.length || select.includes('raylib'))
        format(run('raylib', 'raylib_node_raylib'));
}

function run(name, ref) {
    let tests = [];
    {
        let entries = fs.readdirSync(__dirname);

        let pattern = name + '_*.js';
        let re = minimatch.makeRe(pattern);

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
            throw new Error(proc.stderr);

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

function format(tests) {
    let len0 = tests.reduce((acc, test) => Math.max(acc, test.name.length), 0);

    console.log(`${'Benchmark'.padEnd(len0, ' ')} | Iteration time | Relative performance | Overhead`);
    console.log(`${'-'.padEnd(len0, '-')} | -------------- | -------------------- | --------`);
    for (let test of tests) {
        let time = Math.round(test.time / test.iterations * 10000000) + ' ns';
        let ratio = test.ratio.toFixed(test.ratio < 0.01 ? 3 : 2);
        let overhead = (typeof test.overhead == 'number') ? `${test.overhead >= 0 ? '+' : ''}${test.overhead}%` : test.overhead;

        console.log(`${test.name.padEnd(len0, ' ')} | ${('' + time).padEnd(14, ' ')} | x${('' + ratio).padEnd(19, ' ')} | ${('' + overhead).padEnd(8, ' ')}`);
    }

    console.log('');
}
