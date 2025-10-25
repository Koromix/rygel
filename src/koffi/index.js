// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
