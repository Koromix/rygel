#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const tty = require('tty');
const { style_ansi } = require('../../../tools/qemu/qemu.js');

const TSC_OPTIONS = '--target es2020 --module node16 --allowJs --checkJs --noEmit --resolveJsonModule'.split(' ');

main();

function main() {
    process.chdir(__dirname);

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

    scripts['Sync'] = ['sync.js'];
    scripts['Sync (slow pointers)'] = ['sync.js', '--slow_pointers'];
    scripts['Async'] = ['async.js'];
    scripts['Callbacks'] = ['callbacks.js'];
    scripts['Union'] = ['union.js'];
    if (process.platform != 'win32' && process.platform != 'darwin')
        scripts['POSIX'] = ['posix.js'];
    if (process.platform == 'win32' && process.env.MSYSTEM == null)
        scripts['Win32'] = ['win32.js'];
    if (process.platform != 'darwin')
        scripts['Raylib'] = ['raylib.js'];
    scripts['SQLite'] = ['sqlite.js'];

    for (let key in scripts) {
        let script = scripts[key][0];
        let args = scripts[key].slice(1);

        let filename = path.join(__dirname, script);
        success &= run('Test', key, [filename, ...args]);
    }

    // Make sure tests compile in TypeScript mode
    {
        let tested = new Set;

        for (let key in scripts) {
            let script = scripts[key][0];

            if (tested.has(script))
                continue;
            tested.add(script);

            let filename = path.join(__dirname, script);
            let args = ['../../../vendor/typescript/tsc', ...TSC_OPTIONS, filename];

            success &= run('TypeScript', key, args);
        }
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
