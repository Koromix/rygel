// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import * as esbuild from '../../../vendor/esbuild/wasm';

const BaseMap = [
    null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
    null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null,
    null, null, null, null, null, null, null, null, null, null, null, 0x3E, null, null, null, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, null, null, null, 0x00, null, null,
    null, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, null, null, null, null, null,
    null, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33
];

async function init() {
    let url = `${ENV.urls.static}esbuild/esbuild.wasm`;
    await esbuild.initialize({ wasmURL: url });
}

async function build(code, get_file) {
    let plugin = {
        name: 'goupile',
        setup: build => {
            build.onResolve({ filter: /.*/ }, args => ({
                path: args.path,
                namespace: 'goupile'
            }));

            build.onLoad({ filter: /.*/, namespace: 'goupile' }, async args => {
                if (args.path == '@')
                    return { contents: code, loader: 'js' };

                let file = await get_file(args.path);
                return { contents: file };
            });
        }
    };

    let ret = await esbuild.build({
        entryPoints: ['@'],
        bundle: true,
        format: 'esm',
        write: false,
        outfile: '@',
        target: 'es2022',
        sourcemap: 'external',
        plugins: [plugin]
    })

    let bundle = ret.outputFiles.find(out => out.path == '/@');
    let map = ret.outputFiles.find(out => out.path == '/@.map');

    let mappings = JSON.parse(map.text).mappings;
    let lines = mappings.split(';').map(str => str ? str.split(',').map(decodeVLQ) : null);

    // Convert to absolute offsets
    {
        let acc = [0, 0, 0, 0, 0];

        for (let segments of lines) {
            if (segments == null)
                continue;

            acc[0] = 0;

            for (let seg of segments) {
                acc[0] += seg[0];
                seg[0] = acc[0];

                if (seg.length >= 4) {
                    acc[1] += seg[1];
                    seg[1] = acc[1];
                    acc[2] += seg[2];
                    seg[2] = acc[2];
                    acc[3] += seg[3];
                    seg[3] = acc[3];
                }
                if (seg.length == 5) {
                    acc[4] += seg[4];
                    seg[4] = acc[4];
                }
            }
        }
    }

    let build = {
        code: bundle.text,
        map: lines
    };

    return build;
}

function decodeVLQ(str) {
    let results = [];

    let shift = 0;
    let acc = 0;

    for (let i = 0; i < str.length; i += 1) {
        let value = BaseMap[str.charCodeAt(i)];

        if (value == null)
            throw new Error('Invalid VLQ');

        acc += (value & 0x1F) << shift;

        if (value & 0x20) {
            shift += 5;
            continue;
        }

        if (acc & 1) {
            acc >>>= 1;
            results.push(!acc ? -0x80000000 : -acc);
        } else {
            acc >>>= 1;
            results.push(acc);
        }

        acc = 0;
        shift = 0;
    }

    return results;
}

function mapLine(map, line, column) {
    line--;
    column--;

    let segments = map[line];

    if (segments == null)
        return null;

    let match = null;

    for (let seg of segments) {
        if (seg[2] != null)
            match = seg[2] + 1;
        if (column <= seg[0])
            break;
    }

    return match;
}

export {
    init,
    build,
    mapLine
}
