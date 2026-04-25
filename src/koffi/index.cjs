// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const { detectPlatform, loadDynamic, wrapNative } = require('./src/init.js');

let [version, pkg, triplets] = detectPlatform();
let native = loadDynamic(__dirname + '/../..', pkg, triplets);

if (native.version != version)
    throw new Error('Mismatched native Koffi modules');

wrapNative(native);

module.exports = native;
