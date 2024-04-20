// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

import * as esbuild from '../../../vendor/esbuild/wasm';

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
                if (args.path == '@.js')
                    return { contents: code, loader: 'js' };

                let file = await get_file(args.path);
                return { contents: file };
            });
        }
    };

    let ret = await esbuild.build({
        entryPoints: ['@.js'],
        bundle: true,
        write: false,
        outfile: 'bundle.js',
        target: 'es6',
        plugins: [plugin]
    })

    let bundle = ret.outputFiles[0].text;
    return bundle;
}

export {
    init,
    build
}
