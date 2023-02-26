// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

'use strict';

const package = require('../package.json');
const cnoke = require('../../cnoke');
const util = require('util');

if (process.versions.napi == null || process.versions.napi < 8) {
    let major = parseInt(process.versions.node, 10);
    let required = cnoke.get_napi_version(8, major);

    if (required != null) {
        throw new Error(`Project ${pkg.name} requires Node >= ${required} in the Node ${major}.x branch (N-API >= 8)`);
    } else {
        throw new Error(`Project ${pkg.name} does not support the Node ${major}.x branch (N-API < 8)`);
    }
}

let arch = cnoke.determine_arch();
let filename = __dirname + `/../build/${package.version}/koffi_${process.platform}_${arch}/koffi.node`;

// Development build
if (!fs.existsSync(filename)) {
    let alternative = __dirname + '/../build/koffi.node';
    if (fs.existsSync(alternative))
        filename = alternative;
}

let native = require(filename);

module.exports = {
    ...native,

    // Deprecated functions
    handle: util.deprecate(native.opaque, 'The koffi.handle() function was deprecated in Koffi 2.1, use koffi.opaque() instead', 'KOFFI001')
};
