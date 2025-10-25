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
    let root_dir = path.join(__dirname, cnoke.output);

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
    if (process.platform.toString() == 'linux' || process.platform.toString() == 'macos') {
        assert.equal(GetInt1(), 1);
        assert.equal(GetInt2(), 1);
        assert.equal(GetInt3(), 3);
    }
}
