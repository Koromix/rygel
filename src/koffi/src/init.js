// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'fs';
import path from 'path';
import { createRequire } from 'node:module';
import { determineAbi } from '../../cnoke/src/abi.js';
import PACKAGE from '../package.json' with { type: 'json' };

const requireNative = createRequire(import.meta.dirname);

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

    if (process.resourcesPath != null)
        roots.push(process.resourcesPath);

    let filenames = roots.flatMap(dir => triplets.flatMap(triplet => [
        `${dir}/build/koffi/${triplet}/koffi.node`,
        `${dir}/bin/Koffi/${triplet}/koffi.node`,
        `${dir}/node_modules/koffi/build/koffi/${triplet}/koffi.node`,
        `${dir}/../@koromix/koffi-${pkg}/${triplet}/koffi.node`
    ]));

    for (let filename of filenames) {
        if (!fs.existsSync(filename))
            continue;

        try {
            native = requireNative(filename);
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

export {
    detectPlatform,
    loadDynamic
}
