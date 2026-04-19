// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

'use strict';

const { detect, init } = require('./src/init.js');
const { find } = require('./src/static.js');

let [pkg, triplets] = detect();
let native = find(pkg);
let mod = init(pkg, triplets, native);

module.exports = mod;
