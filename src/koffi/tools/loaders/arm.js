// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { determineAbi } from '../../../cnoke/src/abi.js';

if (determineAbi() != 'armhf')
    throw new Error('The ARM32 prebuild only supports the hard-float ABI (armhf)');

let filename = './linux_armhf/koffi.node';
module.exports = require(filename);
