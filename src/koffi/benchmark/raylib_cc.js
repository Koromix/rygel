// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const { spawnSync } = require('child_process');
const path = require('path');
const { performance } = require('perf_hooks');
const pkg = require('./package.json');

main();

function main() {
    let filename = path.join(__dirname, pkg.cnoke.output, 'raylib_cc' + (process.platform == 'win32' ? '.exe' : ''));
    let proc = spawnSync(filename, process.argv.slice(2), { stdio: 'inherit' });

    if (proc.status == null) {
        console.error(proc.error);
        process.exit(1);
    }

    process.exit(proc.status);
}
