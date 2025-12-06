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

    let name = 'staks';
    let output_dir = '../../bin/Web/' + name;

    let src_filenames = [
        './src/index.html',
        './src/game.js'
    ];

    let ctx = await esbuild.context({
        absWorkingDir: process.cwd(),
        entryPoints: src_filenames,
        entryNames: '[name]',
        logLevel: 'info',
        bundle: true,
        minify: !watch,
        sourcemap: watch ? 'inline' : false,
        tsconfigRaw: JSON.stringify({
            compilerOptions: {
                baseUrl: __dirname + '/../..'
            },
        }),
        format: 'esm',
        target: 'es2020',
        loader: {
            '.woff2': 'dataurl',
            '.png': 'file',
            '.webp': 'file',
            '.webm': 'file',
            '.mp3': 'file'
        },
        outdir: output_dir + '/static',
        plugins: [
            {
                name: 'assemble',
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
                        if (fs.existsSync(output_dir + '/static/index.html'))
                            fs.renameSync(output_dir + '/static/index.html', output_dir + '/index.html');
                        fs.copyFileSync(`./assets/${name}.png`, output_dir + '/favicon.png');
                        fs.copyFileSync('./src/manifest.json', output_dir + '/manifest.json');
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

function printUsage() {
    let basename = path.basename(__filename);
    console.log(`${basename} [--watch]`);
}
