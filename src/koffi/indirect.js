// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

'use strict';

const { detect, init } = require('./src/init.js');

let [pkg, triplet] = detect();
let mod = init(pkg, triplet, null);

module.exports = mod;
