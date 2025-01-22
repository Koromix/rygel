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
const esbuild = require('../../../vendor/esbuild/native');
const Mustache = require('../../../vendor/mustache');
const experiments = require('./src/track/experiments/experiments.json');

let watch = false;

main();

async function main() {
    for (let i = 2; i < process.argv.length; i++) {
        let arg = process.argv[i];

        switch (arg) {
            case '--watch': { watch = true; } break;
            case '--help': {
                printUsage();
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

    try {
        await run();
        process.exit(0);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function printUsage() {
    let basename = path.basename(__filename);
    console.log(`${basename} [--watch]`);
}

async function run() {
    process.chdir(__dirname);

    let meta = await build();

    if (watch) {
        for (;;) {
            let inputs = Object.keys(meta.inputs);

            await Promise.any(inputs.map(input => {
                let p = new Promise((resolve, reject) => fs.watch(input, {}, resolve));
                return p;
            }));

            meta = await build();
        }
    }
}

async function build() {
    let src_filenames = [
        './src/app.js',

        '../../../vendor/sqlite3mc/wasm/jswasm/sqlite3.wasm',
        '../../../vendor/sqlite3mc/wasm/jswasm/sqlite3-opfs-async-proxy.js',
        '../../../vendor/sqlite3mc/wasm/jswasm/sqlite3-worker1-bundler-friendly.mjs',
        '../../../vendor/notion/notion.json',

        ...experiments.map(exp => `./src/track/experiments/${exp.key}/${exp.key}.js`)
    ];

    // Work around hard-coded URLs by moving these files to a common subdirectory
    // with a changing prefix
    let fixed_names = [
        'sqlite3.wasm',
        'sqlite3-opfs-async-proxy.js',
        'sqlite3-worker1-bundler-friendly.mjs'
    ];
    let fixed_prefix = createRandomStr(8);

    let result = await esbuild.build({
        entryPoints: src_filenames,
        assetNames: '[name]-[hash]',
        entryNames: '[name]-[hash]',
        logLevel: 'info',
        bundle: true,
        minify: !watch,
        sourcemap: watch ? 'inline' : false,
        format: 'esm',
        target: 'es2020',
        metafile: true,
        loader: {
            '.png': 'file',
            '.webp': 'file',
            '.woff2': 'file',
            '.wasm': 'copy'
        },
        outdir: 'dist/static/',
        plugins: [
            {
                name: 'html',
                setup: build => {
                    build.onStart(() => {
                        fs.rmSync('dist/static/', { recursive: true, force: true });
                    });

                    build.onEnd(result => {
                        let bundles = {};
                        let script = null;
                        let style = null;

                        for (let dest in result.metafile.outputs) {
                            let obj = result.metafile.outputs[dest];

                            if (!dest.startsWith('dist/'))
                                continue;

                            if (obj.entryPoint == 'src/app.js') {
                                script = dest.substr(5);
                                style = obj.cssBundle.substr(5);
                            } else if (obj.entryPoint != null) {
                                let basename = path.basename(obj.entryPoint);

                                if (fixed_names.includes(basename)) {
                                    let dirname = 'dist/static/' + fixed_prefix;
                                    let alternative = dirname + '/' + basename;

                                    fs.mkdirSync(dirname, { recursive: true });
                                    fs.renameSync(dest, alternative);

                                    bundles[basename] = alternative.substr(4);
                                } else {
                                    bundles[basename] = dest.substr(4);
                                }
                            }
                        }

                        let template = fs.readFileSync('./src/index.html', { encoding: 'UTF-8' });

                        let html = Mustache.render(template, {
                            bundles: JSON.stringify(bundles),
                            script: script,
                            style: style
                        });

                        fs.writeFileSync('./dist/index.html', html);
                        fs.copyFileSync('../assets/ldv.webp', './dist/favicon.webp');
                    });
                }
            }
        ]
    });

    return result.metafile;
}

function createRandomStr(len) {
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';

    let str = '';
    for (let i = 0; i < len; i++) { 
        let rnd = Math.floor(Math.random() * chars.length);
        str += chars.charAt(rnd);
    }

    return str;
}
