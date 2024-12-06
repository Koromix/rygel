#!/usr/bin/env node

// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

const fs = require('fs');
const path = require('path');
const esbuild = require('../../../vendor/esbuild/wasm');
const experiments = require('../src/track/experiments/experiments.json');

let watch = false;

for (let i = 2; i < process.argv.length; i++) {
    let arg = process.argv[i];

    switch (arg) {
        case '--watch': { watch = true; } break;
        case '--help': {
            print_usage();
            process.exit(0);
        } break;

        default: {
            if (arg.startsWith('-')) {
                console.error(`Invalid option '${arg}'`);
                process.exit(1);
            }
        } break;
    }
}

main();

async function main() {
    try {
        await run();
        process.exit(0);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function run() {
    process.chdir(__dirname + '/..');

    let src_filenames = [
        './ludivine.js',
        '../../vendor/sqlite3/wasm/jswasm/sqlite3.wasm',
        '../../vendor/sqlite3/wasm/jswasm/sqlite3-opfs-async-proxy.js',
        '../../vendor/sqlite3/wasm/jswasm/sqlite3-worker1-bundler-friendly.mjs',

        ...experiments.map(exp => `./src/track/experiments/${exp.key}/${exp.key}.js`)
    ];

    let ctx = await esbuild.context({
        entryPoints: src_filenames,
        entryNames: '[name]',
        logLevel: 'info',
        bundle: true,
        minify: !watch,
        sourcemap: watch ? 'inline' : false,
        format: 'esm',
        target: 'es2020',
        loader: {
            '.png': 'file',
            '.webp': 'file',
            '.wasm': 'copy',
            '.woff2': 'file'
        },
        outdir: 'build/'
    });

    await ctx.rebuild();

    if (watch) {
        await ctx.watch();
        await new Promise(() => {});
    }
}

function print_usage() {
    let basename = path.basename(__filename);
    console.log(`${basename} [--watch]`);
}
