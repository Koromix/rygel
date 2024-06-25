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

const fs = require('fs');
const { get_napi_version, determine_arch } = require('../cnoke/src/tools.js');
const { wrap } = require('./src/wrap.js');
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
    let pairs = [`${process.platform}_${arch}`];

    if (process.resourcesPath != null)
        roots.push(process.resourcesPath);
    if (process.platform == 'linux')
        pairs.push(`musl_${arch}`);

    let filenames = roots.flatMap(root => pairs.flatMap(pair => [
        `${root}/build/koffi/${pair}/koffi.node`,
        `${root}/koffi/${pair}/koffi.node`,
        `${root}/node_modules/koffi/build/koffi/${pair}/koffi.node`,
        `${root}/../../bin/Koffi/${pair}/koffi.node`
    ]));

    let first_err = null;

    for (let filename of filenames) {
        if (!fs.existsSync(filename))
            continue;

        try {
            // Trick so that webpack does not try to do anything with require() call
            native = eval('require')(filename);
        } catch (err) {
            if (first_err == null)
                first_err = err;
            continue;
        }

        break;
    }

    if (first_err != null)
        throw first_err;
}

if (native == null)
    throw new Error('Cannot find the native Koffi module; did you bundle it correctly?');
if (native.version != pkg.version)
    throw new Error('Mismatched native Koffi modules');

module.exports = wrap(native);
