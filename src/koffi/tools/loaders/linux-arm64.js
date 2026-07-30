// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { determineLibc } from '../../../cnoke/src/abi.js';

const BINARIES = {
    musl: './musl_arm64/koffi.node',
    glibc: './linux_arm64/koffi.node'
};

let last_err = null;

try {
    let libc = determineLibc();
    module.exports = require(BINARIES[libc]);
} catch (err) {
    last_err = err;
}

if (module.exports?.version == null) {
    // Something went wrong (could not determine libc, could not load the library).
    // Try them all now. Starts with musl to skip glibc compatibility layer in musl distributions.

    for (let filename of Object.values(BINARIES)) {
        try {
            module.exports = require(filename);
            break;
        } catch (err) {
            last_err = err;
        }
    }
}

if (module.exports?.version == null) {
    let err = last_err ?? new Error('Could not load any existing prebuilt binary');
    throw err;
}
