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
const esbuild = require('../../vendor/esbuild/wasm');
const Mustache = require('../../vendor/mustache');

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
    let src_filenames = [
        './src/main.js'
    ];

    let html = {
        name: 'html',
        setup: build => {
            build.onEnd(result => {
              console.log(`build ended with ${result.errors.length} errors`);
            });
        }
    };

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
            '.woff2': 'dataurl',
            '.png': 'file',
            '.webp': 'file',
            '.ogg': 'file',
            '.mp3': 'file'
        },
        outdir: 'dist/static/',
        plugins: [
            {
                name: 'html',
                setup: build => {
                    build.onEnd(result => {
                        let template = fs.readFileSync('./src/index.html', { encoding: 'UTF-8' });
                        let html = Mustache.render(template, { buster: (new Date).valueOf() });

                        fs.writeFileSync('./dist/index.html', html);
                    });
                }
            }
        ]
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
