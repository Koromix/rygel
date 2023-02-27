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

const cnoke = require('../../cnoke/src/index.js');
const util = require('util');
const fs = require('fs');

const pkg = (() => {
    try {
        return require('../../../package.json');
    } catch (err) {
        return require('../package.json');
    }
})();

if (process.versions.napi == null || process.versions.napi < pkg.cnoke.napi) {
    let major = parseInt(process.versions.node, 10);
    let required = cnoke.get_napi_version(pkg.cnoke.napi, major);

    if (required != null) {
        throw new Error(`Project ${pkg.name} requires Node >= ${required} in the Node ${major}.x branch (N-API >= ${pkg.cnoke.napi})`);
    } else {
        throw new Error(`Project ${pkg.name} does not support the Node ${major}.x branch (N-API < ${pkg.cnoke.napi})`);
    }
}

let arch = cnoke.determine_arch();
let filename = __dirname + `/../build/${pkg.version}/koffi_${process.platform}_${arch}/koffi.node`;

// Development build
if (!fs.existsSync(filename)) {
    let dev_filename = __dirname + '/../build/koffi.node';

    if (fs.existsSync(dev_filename))
        filename = dev_filename;
}

let native = require(filename);

module.exports = {
    ...native,

    // Deprecated functions
    handle: util.deprecate(native.opaque, 'The koffi.handle() function was deprecated in Koffi 2.1, use koffi.opaque() instead', 'KOFFI001')
};
