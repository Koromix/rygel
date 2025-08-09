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
const tty = require('tty');
const { style_ansi } = require('../../../tools/qemu/qemu.js');

const TSC_OPTIONS = '--target es2020 --module node16 --allowJs --checkJs --noEmit --resolveJsonModule'.split(' ');

main();

function main() {
    try {
        if (!test())
            process.exit(1);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function test() {
    let scripts = {};
    let success = true;

    scripts.Sync = 'sync.js';
    scripts.Async = 'async.js';
    scripts.Callbacks = 'callbacks.js';
    scripts.Union = 'union.js';
    if (process.platform != 'win32' && process.platform != 'darwin')
        scripts.POSIX = 'posix.js';
    if (process.platform == 'win32')
        scripts.Win32 = 'win32.js';
    if (process.platform != 'darwin')
        scripts.Raylib = 'raylib.js';
    scripts.SQLite = 'sqlite.js';

    for (let key in scripts) {
        let filename = path.join(__dirname, scripts[key]);
        success &= run('Test', key, [filename]);
    }

    for (let key in scripts) {
        let filename = path.join(__dirname, scripts[key]);
        let args = ['../../../vendor/typescript/bin/tsc', ...TSC_OPTIONS, filename];

        success &= run('TypeScript', key, args);
    }

    return success;
}

function run(action, title, args) {
    let start = process.hrtime.bigint();
    let proc = spawnSync(process.execPath, args);

    try {
        if (proc.status == null)
            throw new Error(proc.error);
        if (proc.status !== 0) {
            let stdout = proc.stdout.toString().trim();
            let stderr = proc.stderr.toString().trim();

            throw new Error(stderr || stdout);
        }

        let time = Number((process.hrtime.bigint() - start) / 1000000n);
        console.log(`>> ${action} ${title} ${style_ansi('[' + (time / 1000).toFixed(2) + 's]', 'green bold')}`);

        return true;
    } catch (err) {
        console.log(`>> ${action} ${title} ${style_ansi('[error]', 'red bold')}`);

        let str = '\n' + style_ansi(err.message.replace(/^/gm, ' '.repeat(3)), '33');
        console.log(str);

        return false;
    }
}
