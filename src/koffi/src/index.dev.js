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

const { get_napi_version } = require('../../cnoke/src/tools.js');
const pkg = require('../package.json');

if (process.versions.napi == null || process.versions.napi < pkg.cnoke.napi) {
    let major = parseInt(process.versions.node, 10);
    let required = get_napi_version(pkg.cnoke.napi, major);

    if (required != null) {
        throw new Error(`Project ${pkg.name} requires Node >= ${required} in the Node ${major}.x branch (N-API >= ${pkg.cnoke.napi})`);
    } else {
        throw new Error(`Project ${pkg.name} does not support the Node ${major}.x branch (N-API < ${pkg.cnoke.napi})`);
    }
}

let dev_filename = __dirname + '/../build/koffi.node';
let native = require(filename);

module.exports = native;
