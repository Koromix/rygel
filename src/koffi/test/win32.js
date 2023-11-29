#!/usr/bin/env node

// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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
const util = require('util');

const CallThroughFunc = koffi.proto('int CallThroughFunc()');

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
    // Test relative path because on Windows this uses a different code path
    process.chdir(__dirname);

    let lib_filename = './build/win32.dll';
    let lib = koffi.load(lib_filename);

    let kernel32 = koffi.load('kernel32.dll');

    const DivideBySafe = lib.func('int DivideBySafe(int a, int b)');
    const CallThrough = lib.func('int CallThrough(CallThroughFunc *func)');

    const Sleep = kernel32.func('void __stdcall Sleep(uint32_t dwMilliseconds)');
    const GetLastError = kernel32.func('uint32_t __stdcall GetLastError()');
    const SetLastError = kernel32.func('void __stdcall SetLastError(uint32_t dwErrorCode)');

    // Try to make sure GetLastError() survives across Koffi calls
    for (let i = 0; i < 100; i++) {
        SetLastError(i);
        Sleep(0);
        assert.equal(GetLastError(), i);
        assert.equal(GetLastError(), i);
    }

    // Sync SEH support
    assert.equal(DivideBySafe(12, 3), 4);
    assert.equal(DivideBySafe(84, 0), -42);

    // Async SEH support
    {
        let results = await Promise.all([
            util.promisify(DivideBySafe.async)(90, -9),
            util.promisify(DivideBySafe.async)(227, 0)
        ]);

        assert.equal(results[0], -10);
        assert.equal(results[1], -42);
    }

    // Test SEH inside callback
    assert.equal(CallThrough(() => DivideBySafe(16, 2)), 8);
    assert.equal(CallThrough(() => DivideBySafe(16 * 2, 0)), -42);

    // Test SEH inside callback running async
    {
        let results = await Promise.all([
            util.promisify(CallThrough.async)(() => DivideBySafe(75, -5)),
            util.promisify(CallThrough.async)(() => DivideBySafe(7, 5 - 5))
        ]);

        assert.equal(results[0], -15);
        assert.equal(results[1], -42);
    }
}
