// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

'use strict';

const { detect, init } = require('./src/init.js');

let triplet = detect();

let native = null;

// Try an explicit list with static strings to help bundlers
try {
    switch (triplet) {
        case 'darwin_arm64': { native = require('./build/koffi/darwin_arm64/koffi.node'); } break;
        case 'darwin_x64': { native = require('./build/koffi/darwin_x64/koffi.node'); } break;
        case 'freebsd_arm64': { native = require('./build/koffi/freebsd_arm64/koffi.node'); } break;
        case 'freebsd_ia32': { native = require('./build/koffi/freebsd_ia32/koffi.node'); } break;
        case 'freebsd_x64': { native = require('./build/koffi/freebsd_x64/koffi.node'); } break;
        case 'linux_armhf': { native = require('./build/koffi/linux_armhf/koffi.node'); } break;
        case 'linux_arm64': { native = require('./build/koffi/linux_arm64/koffi.node'); } break;
        case 'linux_ia32': { native = require('./build/koffi/linux_ia32/koffi.node'); } break;
        case 'linux_loong64': { native = require('./build/koffi/linux_loong64/koffi.node'); } break;
        case 'linux_riscv64d': { native = require('./build/koffi/linux_riscv64d/koffi.node'); } break;
        case 'linux_x64': { native = require('./build/koffi/linux_x64/koffi.node'); } break;
        case 'openbsd_ia32': { native = require('./build/koffi/openbsd_ia32/koffi.node'); } break;
        case 'openbsd_x64': { native = require('./build/koffi/openbsd_x64/koffi.node'); } break;
        case 'win32_arm64': { native = require('./build/koffi/win32_arm64/koffi.node'); } break;
        case 'win32_ia32': { native = require('./build/koffi/win32_ia32/koffi.node'); } break;
        case 'win32_x64': { native = require('./build/koffi/win32_x64/koffi.node'); } break;
    }
} catch {
    // We don't try to detect musl distributions, just fallback to it if the GNU build failed

    try {
        switch (triplet) {
            case 'linux_arm64': { native = require('./build/koffi/musl_arm64/koffi.node'); } break;
            case 'linux_x64': { native = require('./build/koffi/musl_x64/koffi.node'); } break;
        }
    } catch {
        // Go on
    }
}

let mod = init(triplet, native);

module.exports = mod;
