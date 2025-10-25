#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const koffi = require('../../koffi');
const assert = require('assert');
const path = require('path');
const util = require('util');
const { cnoke } = require('./package.json');

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
    // Test relative path because on Windows this uses a different code path
    process.chdir(__dirname);

    let lib_filename = cnoke.output + '/win32.dll';
    let lib = koffi.load(lib_filename);

    let kernel32 = koffi.load('kernel32.dll');

    const DivideBySafe1 = lib.cdecl('DivideBySafe1', 'int', ['int', 'int']);
    const DivideBySafe2 = lib.cdecl('DivideBySafe2', 'int', ['int', 'int']);
    const CallThrough1 = lib.func('int CallThrough(CallThroughFunc1 *func, int value)');
    const CallThrough2 = lib.func('int CallThrough(CallThroughFunc2 *func, int value)');

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

    // Sync SEH and exception support
    assert.equal(DivideBySafe1(12, 3), 4);
    assert.equal(DivideBySafe1(84, 0), -42);
    assert.equal(DivideBySafe2(12, 3), 4);
    assert.equal(DivideBySafe2(84, 0), -42);

    // Async SEH support
    {
        let results = await Promise.all([
            util.promisify(DivideBySafe1.async)(90, -9),
            util.promisify(DivideBySafe1.async)(227, 0),
            util.promisify(DivideBySafe2.async)(90, -9),
            util.promisify(DivideBySafe2.async)(227, 0)
        ]);

        assert.equal(results[0], -10);
        assert.equal(results[1], -42);
        assert.equal(results[2], -10);
        assert.equal(results[3], -42);
    }

    // Test SEH inside callback
    assert.equal(CallThrough1((value) => DivideBySafe1(value, 2), 16), 8);
    assert.equal(CallThrough2((value) => DivideBySafe1(value * 2, 0), 16), -42);
    assert.equal(CallThrough1((value) => DivideBySafe2(value, 2), 16), 8);
    assert.equal(CallThrough2((value) => DivideBySafe2(value * 2, 0), 16), -42);

    // Test SEH inside callback running async
    {
        let results = await Promise.all([
            util.promisify(CallThrough1.async)((value) => DivideBySafe1(value + 5, -5), 70),
            util.promisify(CallThrough2.async)((value) => DivideBySafe1(value + 2, 5 - 5), 5),
            util.promisify(CallThrough1.async)((value) => DivideBySafe2(value + 5, -5), 70),
            util.promisify(CallThrough2.async)((value) => DivideBySafe2(value + 2, 5 - 5), 5)
        ]);

        assert.equal(results[0], -15);
        assert.equal(results[1], -42);
        assert.equal(results[2], -15);
        assert.equal(results[3], -42);
    }
}
