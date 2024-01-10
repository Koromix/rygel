// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in 
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const util = require('util');
const fs = require('fs');
const { get_napi_version, determine_arch } = require('../cnoke/src/tools.js');
const pkg = require('./package.json');

if (process.versions.napi == null || process.versions.napi < pkg.cnoke.napi) {
    let major = parseInt(process.versions.node, 10);
    let required = get_napi_version(pkg.cnoke.napi, major);

    if (required != null) {
        throw new Error(`This engine is based on Node ${process.versions.node}, but ${pkg.name} requires Node >= ${required} in the Node ${major}.x branch (N-API >= ${pkg.cnoke.napi})`);
    } else {
        throw new Error(`This engine is based on Node ${process.versions.node}, but ${pkg.name} does not support the Node ${major}.x branch (N-API < ${pkg.cnoke.napi})`);
    }
}

let arch = determine_arch();
let triplet = `${process.platform}_${arch}`;

let native = null;

// Search everywhere we know
{
    let roots = [__dirname];

    if (process.resourcesPath != null)
        roots.push(process.resourcesPath);

    let names = [
        `/build/koffi/${process.platform}_${arch}/koffi.node`,
        `/koffi/${process.platform}_${arch}/koffi.node`,
        `/node_modules/koffi/build/koffi/${process.platform}_${arch}/koffi.node`
    ];

    for (let root of roots) {
        for (let name of names) {
            let filename = root + name;

            if (fs.existsSync(filename)) {
                // Trick so that webpack does not try to do anything with require() call
                native = eval('require')(filename);
                break;
            }
        }

        if (native != null)
            break;
    }
}

if (native == null)
    throw new Error('Cannot find the native Koffi module; did you bundle it correctly?');
if (native.version != pkg.version)
    throw new Error('Mismatched native Koffi modules');

module.exports = {
    ...native,

    // Deprecated functions
    handle: util.deprecate(native.opaque, 'The koffi.handle() function was deprecated in Koffi 2.1, use koffi.opaque() instead', 'KOFFI001'),
    callback: util.deprecate(native.proto, 'The koffi.callback() function was deprecated in Koffi 2.4, use koffi.proto() instead', 'KOFFI002')
};

let load = module.exports.load;

module.exports.load = (...args) => {
    let lib = load(...args);

    lib.cdecl = util.deprecate((...args) => lib.func('__cdecl', ...args), 'The koffi.stdcall() function was deprecated in Koffi 2.7, use koffi.func(...) instead', 'KOFFI003');
    lib.stdcall = util.deprecate((...args) => lib.func('__stdcall', ...args), 'The koffi.stdcall() function was deprecated in Koffi 2.7, use koffi.func("__stdcall", ...) instead', 'KOFFI004');
    lib.fastcall = util.deprecate((...args) => lib.func('__fastcall', ...args), 'The koffi.fastcall() function was deprecated in Koffi 2.7, use koffi.func("__fastcall", ...) instead', 'KOFFI005');
    lib.thiscall = util.deprecate((...args) => lib.func('__thiscall', ...args), 'The koffi.thiscall() function was deprecated in Koffi 2.7, use koffi.func("__thiscall", ...) instead', 'KOFFI006');

    return lib;
};
