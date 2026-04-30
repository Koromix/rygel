// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { detectPlatform, loadDynamic, wrapNative } from './src/init.js';
import { BINARY_ROOT, loadStatic } from './src/static.js';

let [version, pkg, triplets] = detectPlatform();
let native = null;

STATIC: native = loadStatic(pkg);

if (native == null)
    native = loadDynamic(BINARY_ROOT, pkg, triplets);
if (native.version != version)
    throw new Error('Mismatched native Koffi modules');

wrapNative(native);

export default native;
