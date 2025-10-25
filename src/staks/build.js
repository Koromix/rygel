#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const fs = require('fs');
const path = require('path');
const esbuild = require('../../vendor/esbuild/native');
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
    process.chdir(__dirname);

    let src_filenames = [
        './src/index.html',
        './src/game.js'
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
            '.webm': 'file',
            '.mp3': 'file'
        },
        outdir: 'dist/static/',
        plugins: [
            {
                name: 'html',
                setup: build => {
                    build.onLoad({ filter: /\.html$/ }, args => {
                        let template = fs.readFileSync(args.path, { encoding: 'UTF-8' });
                        let html = Mustache.render(template, { buster: (new Date).valueOf() });

                        return {
                            contents: html,
                            loader: 'copy'
                        };
                    });

                    build.onEnd(result => {
                        if (fs.existsSync('./dist/static/index.html'))
                            fs.renameSync('./dist/static/index.html', './dist/index.html');
                        fs.copyFileSync('./assets/staks.png', './dist/favicon.png');
                        fs.copyFileSync('./src/manifest.json', './dist/manifest.json');
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
