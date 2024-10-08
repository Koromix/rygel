#!/usr/bin/env node

// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

const koffi = require('../../koffi');
const assert = require('assert');
const path = require('path');
const pkg = require('./package.json');

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

const CharCallback = koffi.proto('int CharCallback(int idx, char c)');
const BinaryIntFunc = koffi.proto('int BinaryIntFunc(int a, int b)');

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
    const lib_filename = path.join(__dirname, pkg.cnoke.output, 'async' + koffi.extension);
    const lib = koffi.load(lib_filename);

    const ConcatenateToInt1 = lib.func('ConcatenateToInt1', 'int64_t', Array(12).fill('int8_t'));
    const MakePackedBFG = lib.func('PackedBFG __fastcall MakePackedBFG(int x, double y, _Out_ PackedBFG *p, const char *str)');
    const CallMeChar = lib.func('int CallMeChar(CharCallback *func)');
    const GetBinaryIntFunction = lib.func('BinaryIntFunc *GetBinaryIntFunction(const char *name)');

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

    // Call function pointers
    {
        test_binary('add', 4, 5, 9);
        test_binary('substract', 4, 5, -1);
        test_binary('multiply', 3, 8, 24);
        test_binary('divide', 100, 2, 50);

        function test_binary(type, a, b, expected) {
            let p = new Promise((resolve, reject) => {
                try {
                    let ptr = GetBinaryIntFunction(type);
                    let func = koffi.read(ptr, 'BinaryIntFunc *');

                    func.async(a, b, (err, res) => {
                        assert.equal(res, expected);
                    });

                    resolve();
                } catch (err) {
                    reject(err);
                }
            });
            promises.push(p);
        }
    }

    await Promise.all(promises);
}
