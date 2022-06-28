#!/usr/bin/env node

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

const atoi = require('./build/atoi_napi.node');

const strings = [
    '424242',
    'foobar',
    '123456789'
];

let sum = 0;

main();

function main() {
    let iterations = 20000000;

    if (process.argv.length >= 3) {
        iterations = parseInt(process.argv[2], 10);
        if (Number.isNaN(iterations))
            throw new Error('Not a valid number');
        if (iterations < 1)
            throw new Error('Value must be positive');
    }

    let start = performance.now();

    for (let i = 0; i < iterations; i++) {
        sum += atoi.atoi(strings[i % strings.length]);
    }

    let time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}
