#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const koffi = require('..');
const assert = require('node:assert');
const path = require('node:path');
const { promisify } = require('node:util');
const { cnoke } = require('./package.json');

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
    let promises = [];

    if (global.gc != null)
        console.log('Heap usage before tests:', formatMib(process.memoryUsage().heapUsed));

    // Run a bunch of stuff, inside block so most function references disappear after and become GC-eligible
    const Trigger = await stress(promises);

    if (global.gc != null) {
        for (let i = 0; i < 3; i++) {
            global.gc();
            await new Promise(resolve => setTimeout(resolve, 0));
        }
    }

    Trigger();

    if (global.gc != null) {
        for (let i = 0; i < 3; i++) {
            global.gc();
            await new Promise(resolve => setTimeout(resolve, 0));
        }
    }

    await Promise.all(promises);

    if (global.gc != null)
        console.log('Heap usage after tests:', formatMib(process.memoryUsage().heapUsed));
}

async function stress(promises)
{
    const lib_filename = path.join(__dirname, cnoke.output, 'async' + koffi.extension);
    const lib = koffi.load(lib_filename);

    const ConcatenateToInt1 = lib.func('ConcatenateToInt1', 'int64_t', Array(12).fill('int8_t'));
    const MakePackedBFG = lib.func('PackedBFG __fastcall MakePackedBFG(int x, double y, _Out_ PackedBFG *p, const char *str)');
    const CallMeChar = lib.func('int CallMeChar(CharCallback *func)');
    const GetBinaryIntFunction = lib.func('BinaryIntFunc *GetBinaryIntFunction(const char *name)');
    const WaitForTrigger = lib.func('void WaitForTrigger()');
    const Trigger = lib.func('void Trigger()');

    for (let i = 0; i < 100; i++) {
        await Promise.all(promises);
        promises.length = 0;

        // Issue several async calls
        for (let i = 0; i < 32; i++) {
            let p = (async () => {
                let res = await promisify(ConcatenateToInt1.async)(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7);
                assert.equal(res, 561239440687n);
            })();

            promises.push(p);
        }

        // Make a few synchronous calls while the asynchronous ones are still in the air
        assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);
        assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);
        assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);

        // Failed calls
        for (let j = 0; j < 8; j++) {
            await assert.rejects(promisify(ConcatenateToInt1.async)(5, 6, 1, 2, 'hello', 9, 4, 4, 0, 6, 8, 7), /expected number/);
            await assert.rejects(promisify(MakePackedBFG.async)(null, null, null, null), /expected number/);

            let p1 = assert.rejects(new Promise((resolve, reject) => queueMicrotask(() => promisify(ConcatenateToInt1.async)(5, 6, 1, 2, 'hello', 9, 4, 4, 0, 6, 8, 7).then(resolve, reject))), /expected number/);
            let p2 = assert.rejects(new Promise((resolve, reject) => setTimeout(() => promisify(MakePackedBFG.async)(null, null, null, null).then(resolve, reject), 0)), /expected number/);
            promises.push(p1, p2);
        }

        // Async complex call
        {
            let out = {};
            let p = (async () => {
                let res = await promisify(MakePackedBFG.async)(2, 7, out, '__Hello123456789++++foobarFOOBAR!__');

                assert.deepEqual(res, { a: 2, b: 4, c: -25, d: 'X/__Hello123456789++++foobarFOOBAR!__/X', e: 54, inner: { f: 14, g: 5 } });
                assert.deepEqual(out, res);
            })();
            promises.push(p);
        }

        // Call function pointers
        {
            test_binary('add', 4, 5, 9);
            test_binary('substract', 4, 5, -1);
            test_binary('multiply', 3, 8, 24);
            test_binary('divide', 100, 2, 50);

            function test_binary(type, a, b, expected) {
                let p = (async () => {
                    let ptr = GetBinaryIntFunction(type);
                    let func = koffi.decode(ptr, BinaryIntFunc);

                    let res = await promisify(func.async)(a, b)
                    assert.equal(res, expected);
                })();
                promises.push(p);
            }
        }
    }

    // Add blocked calls (until Trigger), we want to make sure FunctionInfo remains alive even if
    // nothing in JS references it, until call is finished.
    // Don't use promisify it seems to prevent collection while the call is running.
    for (let i = 0; i < 8; i++) {
        let p = new Promise(resolve => WaitForTrigger.async(resolve));
        promises.push(p);
    }

    return Trigger;
}

function formatMib(size) {
    return (size / 1024 / 1024).toFixed(1) + ' MiB';
}
