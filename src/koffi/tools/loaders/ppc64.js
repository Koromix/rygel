// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { determineAbi } from '../../../cnoke/src/abi.js';

if (determineAbi() != 'ppc64le')
    throw new Error('The PowerPC64 prebuild only supports ELFv2 Little-Endian systems');

let filename = './linux_ppc64le/koffi.node';
module.exports = require(filename);
