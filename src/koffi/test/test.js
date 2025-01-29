#!/usr/bin/env node

// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
    let success = true;

    success &= run('Sync', 'sync.js');
    success &= run('Async', 'async.js');
    success &= run('Callbacks', 'callbacks.js');
    success &= run('Union', 'union.js');
    if (process.platform != 'win32' && process.platform != 'darwin')
        success &= run('POSIX', 'posix.js');
    if (process.platform == 'win32')
        success &= run('Win32', 'win32.js');
    if (process.platform != 'darwin')
        success &= run('Raylib', 'raylib.js');
    success &= run('SQLite', 'sqlite.js');

    return success;
}

function run(title, name) {
    let start = process.hrtime.bigint();

    let filename = path.join(__dirname, name);
    let proc = spawnSync(process.execPath, [filename]);

    try {
        if (proc.status == null)
            throw new Error(proc.error);
        if (proc.status !== 0)
            throw new Error(proc.stderr || proc.stdout);

        let time = Number((process.hrtime.bigint() - start) / 1000000n);
        console.log(`>> Test ${title} ${style_ansi('[' + (time / 1000).toFixed(2) + 's]', 'green bold')}`);

        return true;
    } catch (err) {
        console.log(`>> Test ${title} ${style_ansi('[error]', 'red bold')}`);

        let str = '\n' + style_ansi(err.message.replace(/^/gm, ' '.repeat(3)), '33');
        console.log(str);

        return false;
    }
}
