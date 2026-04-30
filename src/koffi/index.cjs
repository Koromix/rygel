// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const { detectPlatform, loadDynamic, wrapNative } = require('./src/init.js');
const { BINARY_ROOT, loadStatic } = require('./src/static.js');

let [version, pkg, triplets] = detectPlatform();
let native = null;

STATIC: native = loadStatic(pkg);

if (native == null)
    native = loadDynamic(BINARY_ROOT, pkg, triplets);
if (native.version != version)
    throw new Error('Mismatched native Koffi modules');

wrapNative(native);

module.exports = native;
