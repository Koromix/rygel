// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

'use strict';

const fs = require('fs');
const path = require('path');
const util = require('util');
const { getNapiVersion, determineArch } = require('../../cnoke/src/util.js');

const PACKAGE = require('../package.json');

function detect() {
    if (process.versions.napi == null || process.versions.napi < PACKAGE.cnoke.napi) {
        let major = parseInt(process.versions.node, 10);
        let required = getNapiVersion(PACKAGE.cnoke.napi, major);

        if (required != null) {
            throw new Error(`This engine is based on Node ${process.versions.node}, but ${PACKAGE.name} requires Node >= ${required} in the Node ${major}.x branch (N-API >= ${PACKAGE.cnoke.napi})`);
        } else {
            throw new Error(`This engine is based on Node ${process.versions.node}, but ${PACKAGE.name} does not support the Node ${major}.x branch (N-API < ${PACKAGE.cnoke.napi})`);
        }
    }

    let arch = determineArch();

    let pkg = `${process.platform}-${process.arch}`;
    let triplets = [`${process.platform}_${arch}`];

    if (process.platform == 'linux')
        triplets.push(`musl_${arch}`);

    return [pkg, triplets];
}

function init(pkg, triplets, native) {
    let err = null;

    if (native == null) {
        let roots = [path.join(__dirname, '..')];

        if (process.resourcesPath != null)
            roots.push(process.resourcesPath);

        let filenames = roots.flatMap(root => triplets.flatMap(triplet => [
            `${root}/@koromix/koffi-${pkg}/${triplet}/koffi.node`,
            `${root}/koffi/build/koffi/${triplet}/koffi.node`,
            `${root}/build/koffi/${triplet}/koffi.node`,
            `${root}/koffi/${triplet}/koffi.node`,
            `${root}/node_modules/koffi/build/koffi/${triplet}/koffi.node`,
            `${root}/../../bin/Koffi/${triplet}/koffi.node`
        ]));

        for (let filename of filenames) {
            if (!fs.existsSync(filename))
                continue;

            try {
                // Trick so that webpack does not try to do anything with require() call
                native = eval('require')(filename);
                break;
            } catch (e) {
                err ??= e;
            }
        }
    }

    if (native == null) {
        err ??= new Error('Cannot find the native Koffi module; did you bundle it correctly?');
        throw err;
    }
    if (native.version != PACKAGE.version)
        throw new Error('Mismatched native Koffi modules');

    let mod = wrap(native);
    return mod;
}

function wrap(native) {
    let obj = {
        ...native,

        // Deprecated functions
        handle: util.deprecate(native.opaque, 'The koffi.handle() function was deprecated in Koffi 2.1, use koffi.opaque() instead', 'KOFFI001'),
        callback: util.deprecate(native.proto, 'The koffi.callback() function was deprecated in Koffi 2.4, use koffi.proto() instead', 'KOFFI002')
    };

    obj.load = (...args) => {
        let lib = native.load(...args);

        lib.cdecl = util.deprecate((...args) => lib.func('__cdecl', ...args), 'The koffi.cdecl() function was deprecated in Koffi 2.7, use koffi.func(...) instead', 'KOFFI003');
        lib.stdcall = util.deprecate((...args) => lib.func('__stdcall', ...args), 'The koffi.stdcall() function was deprecated in Koffi 2.7, use koffi.func("__stdcall", ...) instead', 'KOFFI004');
        lib.fastcall = util.deprecate((...args) => lib.func('__fastcall', ...args), 'The koffi.fastcall() function was deprecated in Koffi 2.7, use koffi.func("__fastcall", ...) instead', 'KOFFI005');
        lib.thiscall = util.deprecate((...args) => lib.func('__thiscall', ...args), 'The koffi.thiscall() function was deprecated in Koffi 2.7, use koffi.func("__thiscall", ...) instead', 'KOFFI006');

        return lib;
    };

    return obj;
}

module.exports = {
    detect,
    init
};
