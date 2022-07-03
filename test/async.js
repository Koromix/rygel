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

const koffi = require('./build/koffi.node');
const assert = require('assert');
const path = require('path');

const PackedBFG = koffi.pack('PackedBFG', {
    a: 'int8_t',
    b: 'int64_t',
    c: 'char',
    d: 'str',
    e: 'short',
    inner: koffi.pack({
        f: 'float',
        g: 'double'
    })
});

main();

async function main() {
    try {
        await test();
        console.log('Success!');
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function test() {
    const lib_filename = path.dirname(__filename) + '/build/misc' + koffi.extension;
    const lib = koffi.load(lib_filename);

    const ConcatenateToInt1 = lib.func('ConcatenateToInt1', 'int64_t', Array(12).fill('int8_t'));
    const MakePackedBFG = lib.func('PackedBFG __fastcall MakePackedBFG(int x, double y, _Out_ PackedBFG *p, const char *str)');

    let promises = [];

    // Issue several async calls
    for (let i = 0; i < 32; i++) {
        let p = new Promise((resolve, reject) => {
            try {
                ConcatenateToInt1.async(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7, (err, res) => {
                    assert.equal(err, null);
                    assert.equal(res, 561239440687n);
                });
                resolve();
            } catch (err) {
                reject(err);
            }
        });
        promises.push(p);
    }

    // Make a few synchronous calls while the asynchronous ones are still in the air
    assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);
    assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);
    assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);

    // Async complex call
    {
        let out = {};
        let p = new Promise((resolve, reject) => {
            try {
                MakePackedBFG.async(2, 7, out, '__Hello123456789++++foobarFOOBAR!__', (err, res) => {
                    assert.deepEqual(res, { a: 2, b: 4, c: -25, d: 'X/__Hello123456789++++foobarFOOBAR!__/X', e: 54, inner: { f: 14, g: 5 } });
                    assert.deepEqual(out, res);
                });
                resolve();
            } catch (err) {
                reject(err);
            }
        });
        promises.push(p);
    }

    await Promise.all(promises);
}
