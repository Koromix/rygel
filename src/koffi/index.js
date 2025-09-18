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

'use strict';

const { detect, init } = require('./src/init.js');

let triplet = detect();

let native = null;

try {
    switch (triplet) {
        case 'darwin_arm64': { native = require('@koromix/koffi-darwin-arm64'); } break;
        case 'darwin_x64': { native = require('@koromix/koffi-darwin-x64'); } break;
        case 'freebsd_arm64': { native = require('@koromix/koffi-freebsd-arm64'); } break;
        case 'freebsd_ia32': { native = require('@koromix/koffi-freebsd-ia32'); } break;
        case 'freebsd_x64': { native = require('@koromix/koffi-freebsd-x64'); } break;
        case 'linux_armhf': { native = require('@koromix/koffi-linux-arm'); } break;
        case 'linux_arm64': { native = require('@koromix/koffi-linux-arm64'); } break;
        case 'linux_ia32': { native = require('@koromix/koffi-linux-ia32'); } break;
        case 'linux_loong64': { native = require('@koromix/koffi-linux-loong64'); } break;
        case 'linux_riscv64d': { native = require('@koromix/koffi-linux-riscv64'); } break;
        case 'linux_x64': { native = require('@koromix/koffi-linux-x64'); } break;
        case 'openbsd_ia32': { native = require('@koromix/koffi-openbsd-ia32'); } break;
        case 'openbsd_x64': { native = require('@koromix/koffi-openbsd-x64'); } break;
        case 'win32_arm64': { native = require('@koromix/koffi-win32-arm64'); } break;
        case 'win32_ia32': { native = require('@koromix/koffi-win32-ia32'); } break;
        case 'win32_x64': { native = require('@koromix/koffi-win32-x64'); } break;
    }
} catch {
    // Go on
}

let mod = init(triplet, native);

module.exports = mod;
