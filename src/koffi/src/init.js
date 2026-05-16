// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import util from 'node:util';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { determineAbi } from '../../cnoke/src/abi.js';
import PACKAGE from '../package.json' with { type: 'json' };

const requireNative = createRequire(import.meta.url);

function detectPlatform() {
    if (process.versions.napi == null || process.versions.napi < PACKAGE.cnoke.napi) {
        let major = parseInt(process.versions.node, 10);
        throw new Error(`This engine is based on Node ${process.versions.node}, but ${PACKAGE.name} does not support the Node ${major}.x branch (N-API < ${PACKAGE.cnoke.napi})`);
    }

    let abi = determineAbi();

    let pkg = `${process.platform}-${process.arch}`;
    let triplets = [`${process.platform}_${abi}`];

    if (process.platform == 'linux')
        triplets.push(`musl_${abi}`);

    return [PACKAGE.version, pkg, triplets];
}

function loadDynamic(root, pkg, triplets) {
    let roots = [root];

    let native = null;
    let err = null;

    if (process.resourcesPath != null) {
        roots.push(process.resourcesPath);
        roots.push(process.resourcesPath + '/node_modules/koffi');
    }

    let names = [
        `${import.meta.dirname}/../../../@koromix/koffi-${pkg}`,
        ...triplets.flatMap(triplet => roots.map(dir => `${dir}/${triplet}/koffi.node`))
    ];

    for (let name of names) {
        if (!fs.existsSync(name))
            continue;

        try {
            native = requireNative(name);
            break;
        } catch (e) {
            err ??= e;
        }
    }

    if (native == null) {
        err ??= new Error('Cannot find the native Koffi module; did you bundle it correctly?');
        throw err;
    }

    return native;
}

function wrapNative(native) {
    let load = native.load;
    let register = native.register;

    native.sizeof = (spec) => native.type(spec).size;
    native.alignof = (spec) => native.type(spec).alignment;

    native.offsetof = (spec, name) => {
        let type = native.type(spec);

        if (type.primitive != 'Record')
            throw new TypeError('The offsetof() function can only be used with record types');

        let member = type.members[name];

        if (member == null)
            throw new Error(`Record type ${type.name} does not have member '${name}'`);

        return member.offset;
    };

    native.register = (...args) => {
        if (args.length >= 3 && typeof args[1] == 'function') {
            process.emitWarning('Using koffi.register() with a custom this value was deprecated in Koffi 3.0, use function.bind() instead', 'DeprecationWarning', 'KOFFI009');

            args[1] = args[1].bind(args[0]);
            args = args.slice(1);
        }

        return register(...args);
    };

    // Deprecated functions
    native.resolve = util.deprecate(native.type, 'The koffi.resolve() function was deprecated in Koffi 3.0, use koffi.type() instead', 'KOFFI007');
    native.introspect = util.deprecate(native.type, 'The koffi.introspect() function was deprecated in Koffi 3.0, use koffi.type() instead', 'KOFFI008');

    native.load = (...args) => {
        let lib = load(...args);

        lib.cdecl = util.deprecate((...args) => lib.func('__cdecl', ...args), 'The koffi.cdecl() function was deprecated in Koffi 2.7, use koffi.func(...) instead', 'KOFFI003');
        lib.stdcall = util.deprecate((...args) => lib.func('__stdcall', ...args), 'The koffi.stdcall() function was deprecated in Koffi 2.7, use koffi.func("__stdcall", ...) instead', 'KOFFI004');
        lib.fastcall = util.deprecate((...args) => lib.func('__fastcall', ...args), 'The koffi.fastcall() function was deprecated in Koffi 2.7, use koffi.func("__fastcall", ...) instead', 'KOFFI005');
        lib.thiscall = util.deprecate((...args) => lib.func('__thiscall', ...args), 'The koffi.thiscall() function was deprecated in Koffi 2.7, use koffi.func("__thiscall", ...) instead', 'KOFFI006');

        return lib;
    };
}

export {
    detectPlatform,
    loadDynamic,
    wrapNative
}
