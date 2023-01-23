#!/usr/bin/env node

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

const koffi = require('./build/koffi.node');

const strings = [
    '424242',
    'foobar',
    '123456789'
];

let sum = 0;

main();

function main() {
    let time = 5000;

    if (process.argv.length >= 3) {
        time = parseFloat(process.argv[2]) * 1000;
        if (Number.isNaN(time))
            throw new Error('Not a valid number');
        if (time < 0)
            throw new Error('Time must be positive');
    }

    let lib = koffi.load(process.platform == 'win32' ? 'msvcrt.dll' : null);

    const atoi = lib.cdecl('atoi', 'int', ['str']);

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        for (let i = 0; i < 1000000; i++)
            sum += atoi(strings[i % strings.length]);

        iterations += 1000000;
    }

    time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}
