#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const tty = require('node:tty');

const TSC_OPTIONS = '--target es2020 --module node16 --allowJs --checkJs --noEmit --resolveJsonModule --strict false'.split(' ');

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

    scripts['Sync'] = { script: 'sync.js' };
    scripts['Sync (debug async)'] = { script: 'sync.js', env: { DEBUG_ASYNC: '1' } };
    scripts['Async'] = { script: 'async.js' };
    scripts['Callbacks'] = { script: 'callbacks.js' };
    scripts['Union'] = { script: 'union.js' };
    if (process.platform != 'win32' && process.platform != 'darwin')
        scripts['POSIX'] = { script: 'posix.js' };
    if (process.platform == 'win32' && process.env.MSYSTEM == null)
        scripts['Win32'] = { script: 'win32.js' };
    if (process.platform != 'darwin')
        scripts['Raylib'] = { script: 'raylib.js' };
    scripts['SQLite'] = { script: 'sqlite.js' };

    for (let key in scripts) {
        let script = scripts[key].script;
        let args = scripts[key].args ?? [];
        let env = scripts[key].env ?? {}

        let filename = path.join(__dirname, script);
        success &= run('Test', key, ['--no-deprecation', filename, ...args], env);
    }

    // Make sure tests compile in TypeScript mode
    {
        let tested = new Set;

        for (let key in scripts) {
            let script = scripts[key].script;

            if (tested.has(script))
                continue;
            tested.add(script);

            let filename = path.join(__dirname, script);
            let args = ['../../../vendor/typescript/tscli', ...TSC_OPTIONS, filename];

            success &= run('TypeScript', key, args);
        }
    }

    return success;
}

function run(action, title, args, env) {
    env = { ...process.env, ...env };

    let start = process.hrtime.bigint();
    let proc = spawnSync(process.execPath, args, { env: env });

    try {
        if (proc.status == null)
            throw new Error(proc.error);
        if (proc.status !== 0) {
            let stdout = proc.stdout.toString().trim();
            let stderr = proc.stderr.toString().trim();

            throw new Error(stderr || stdout);
        }

        let stderr = proc.stderr.toString().trim();

        let time = Number((process.hrtime.bigint() - start) / 1000000n);
        console.log(`>> ${action} ${title} ${styleAnsi('[' + (time / 1000).toFixed(2) + 's]', 'green bold')}`);

        if (stderr) {
            let str = styleAnsi(stderr.replace(/^/gm, ' '.repeat(3)), '33');
            console.log(str);
        }

        return true;
    } catch (err) {
        console.log(`>> ${action} ${title} ${styleAnsi('[error]', 'red bold')}`);

        let str = styleAnsi(err.message.replace(/^/gm, ' '.repeat(3)), '33');
        console.log(str);

        return false;
    }
}

function styleAnsi(text, styles = []) {
    if (!tty.isatty(process.stdout.fd))
        return text;

    if (typeof styles == 'string')
        styles = styles.split(' ');

    let ansi = [];

    for (let style of styles) {
        switch (style) {
            case 'black': { ansi.push('30'); } break;
            case 'red': { ansi.push('31'); } break;
            case 'green': { ansi.push('32'); } break;
            case 'yellow': { ansi.push('33'); } break;
            case 'blue': { ansi.push('34'); } break;
            case 'magenta': { ansi.push('35'); } break;
            case 'cyan': { ansi.push('36'); } break;
            case 'white': { ansi.push('37'); } break;

            case 'gray':
            case 'black+': { ansi.push('90'); } break;
            case 'red+': { ansi.push('91'); } break;
            case 'green+': { ansi.push('92'); } break;
            case 'yellow+': { ansi.push('93'); } break;
            case 'blue+': { ansi.push('94'); } break;
            case 'magenta': { ansi.push('95'); } break;
            case 'cyan+': { ansi.push('96'); } break;
            case 'white+': { ansi.push('97'); } break;

            case 'bold': { ansi.push('1'); } break;
            case 'dim': { ansi.push('2'); } break;
            case 'underline': { ansi.push('4'); } break;
        }
    }

    if (!ansi.length)
        return text;

    let str = `\x1b[${ansi.join(';')}m${text}\x1b[0m`;
    return str;
}
