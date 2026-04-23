// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

'use strict';

const { detect, init } = require('./src/init.js');

let [pkg, triplets] = detect();
let mod = init(__dirname + '/../..', pkg, triplets, null);

module.exports = mod;
