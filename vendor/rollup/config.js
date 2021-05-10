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

import babel from '@rollup/plugin-babel';
import resolve from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
// Tried @rollup/plugin-virtual, it did weird crap
import hypothetical from 'rollup-plugin-hypothetical';

let code;
try {
    const fs = require('fs');
    code = fs.readFileSync(0, 'UTF-8');
} catch (err) {
    // Nothing on stdin (probably)
    code = '';
}

// Quick-and-dirty way to add relevant polyfills
code = `import 'whatwg-fetch';
import '@webcomponents/template';

// Chrome 42 sometimes uses valueOf() for string concatenations, which messes up with
// transpiled template literals. Hack the default concat() implementation to fix this.
String.prototype.concat = function() {
    var str = this;

    for (var i = 0; i < arguments.length; i++) {
        var arg = arguments[i];
        if (arg != null && arg.toString) {
            str = str + arg.toString();
        } else {
            str = str + arg;
        }
    }

    return str;
};

${code}`;

module.exports = {
    input: '__stdin__.js',
    plugins: [
        babel({
            babelrc: false,
            babelHelpers: 'bundled',
            exclude: 'node_modules/**',
            presets: [
                ['@babel/env', {
                    targets: {
                        ie: '11',
                        chrome: '42'
                    },
                    useBuiltIns: 'usage',
                    corejs: 3,
                    modules: false
                }]
            ]
        }),
        resolve(),
        commonjs(),
        hypothetical({
            files: {
                '__stdin__.js': code
            },
            allowFallthrough: true,
            leaveIdsAlone: true
        })
    ]
};
