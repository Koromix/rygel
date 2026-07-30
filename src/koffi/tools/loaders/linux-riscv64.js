// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { determineAbi } from '../../../cnoke/src/abi.js';

if (determineAbi() != 'riscv64d')
    throw new Error('The RISC-V 64 prebuild only supports the LP64D float ABI');

let filename = './linux_riscv64d/koffi.node';
module.exports = require(filename);
