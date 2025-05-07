#!/usr/bin/env node

// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
const util = require('util');
const pkg = require('./package.json');

const CallThroughFunc1 = koffi.proto('int __stdcall CallThroughFunc1(int)');
const CallThroughFunc2 = koffi.proto('__stdcall', 'CallThroughFunc2', 'int', ['int']);

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
    let root_dir = path.join(__dirname, pkg.cnoke.output);

    let lib1 = koffi.load(root_dir + '/posix1.so', { global: true });
    assert.throws(() => koffi.load(root_dir + '/posix2.so'));
    let lib2 = koffi.load(root_dir + '/posix2.so', { lazy: true });
    let lib3 = koffi.load(root_dir + '/posix3.so', { deep: true });

    const SumInts = lib2.func('int SumInts(int a, int b)');
    const GetInt1 = lib1.func('int GetInt()');
    const GetInt2 = lib2.func('int GetInt()');
    const GetInt3 = lib3.func('int GetInt()');

    assert.equal(SumInts(4, 2), 6);
    assert.equal(SumInts(50, -8), 42);

    // Test RTLD_DEEPBIND
    if (process.platform == 'linux' || process.platform == 'macos') {
        assert.equal(GetInt1(), 1);
        assert.equal(GetInt2(), 1);
        assert.equal(GetInt3(), 3);
    }
}
