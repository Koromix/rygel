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
