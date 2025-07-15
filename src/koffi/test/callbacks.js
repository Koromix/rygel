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
const { cnoke } = require('./package.json');

// We need to change this on Windows because the DLL CRT might
// not (probably not) match the one used by Node.js!
let free_ptr = koffi.free;

const StrFree = koffi.disposable('str_free', koffi.types.str, ptr => free_ptr(ptr));
const Str16Free = koffi.disposable('str16_free', 'str16', ptr => free_ptr(ptr));

const BFG = koffi.struct('BFG', {
    a: 'int8_t',
    e: [8, 'short'],
    b: 'int64_t',
    c: 'char',
    d: 'const char *',
    inner: koffi.struct({
        f: 'float',
        g: 'double'
    })
});

const Vec2 = koffi.struct('Vec2', {
    x: 'double',
    y: 'double'
});

const SimpleCallback = koffi.proto('int SimpleCallback(const char *str)');
const RecursiveCallback = koffi.proto('RecursiveCallback', 'float', ['int', 'str', 'double']);
const BigCallback = koffi.proto('BFG BigCallback(BFG bfg)');
const SuperCallback = koffi.proto('void SuperCallback(int i, int v1, double v2, int v3, int v4, int v5, int v6, float v7, int v8)');
const ApplyCallback = koffi.proto('int __stdcall ApplyCallback(int a, int b, int c)');
const IntCallback = koffi.proto('int IntCallback(int x)');
const VectorCallback = koffi.proto('int VectorCallback(int len, Vec2 *vec)');
const SortCallback = koffi.proto('int SortCallback(const void *ptr1, const void *ptr2)');
const CharCallback = koffi.proto('int CharCallback(int idx, char c)');
const StructCallbacks = koffi.struct('StructCallbacks', {
    first: koffi.pointer(IntCallback),
    second: 'IntCallback *',
    third: 'IntCallback *'
});
const RepeatCallback = koffi.proto('void RepeatCallback(int *repeat, const char **str)');
const IdleCallback = koffi.proto('void IdleCallback(void)');

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
    const lib_filename = path.join(__dirname, cnoke.output, 'callbacks' + koffi.extension);
    const lib = koffi.load(lib_filename);

    const CallFree = lib.func('void CallFree(void *ptr)');
    const CallJS = lib.func('int CallJS(const char *str, SimpleCallback *cb)');
    const CallRecursiveJS = lib.func('float CallRecursiveJS(int i, RecursiveCallback *func)');
    const ModifyBFG = lib.func('BFG ModifyBFG(int x, double y, const char *str, BigCallback *func, _Out_ BFG *p)');
    const Recurse8 = lib.func('void Recurse8(int i, SuperCallback *func)');
    const ApplyStd = lib.func('int ApplyStd(int a, int b, int c, ApplyCallback *func)');
    const ApplyMany = lib.func('int ApplyMany(int x, IntCallback **funcs, int length)');
    const ApplyStruct = lib.func('int ApplyStruct(int x, StructCallbacks callbacks)');
    const SetIndirect = lib.func('void SetIndirect(IntCallback *func)');
    const CallIndirect = lib.func('int CallIndirect(int x)');
    const CallThreaded = lib.func('int CallThreaded(IntCallback *func, int x)');
    const MakeVectors = lib.func('int MakeVectors(int len, VectorCallback *func)');
    const MakeVectorsIndirect = lib.func('void MakeVectorsIndirect(int len, VectorCallback *func, _Out_ Vec2 *out)');
    const CallQSort = lib.func('void CallQSort(_Inout_ void *base, size_t nmemb, size_t size, void *cb)');
    const CallMeChar = lib.func('int CallMeChar(CharCallback *func)');
    const GetMinusOne1 = lib.func('int8_t GetMinusOne1(void)');
    const FmtRepeat = lib.func('str_free FmtRepeat(RepeatCallback *cb)');
    const SetIdle = lib.func('void SetIdle(void *env, IdleCallback *cb)');
    const TriggerIdle = lib.func('void TriggerIdle(void)');

    free_ptr = CallFree;

    // Start with test of callback inside async function to make sure the async broker is registered,
    // and avoid regression (see issue #74).
    {
        let chars = [97, 98];

        let cb = koffi.register((idx, c) => (idx + 1) * c, 'CharCallback *');
        let ret = await util.promisify(CallMeChar.async)(cb);

        assert.equal(ret, 97 + 2 * 98);

        // Don't unregister to make lingering trampolines don't make node crash on exit
        // koffi.unregister(cb);
    }

    // Simple test similar to README example
    {
        let ret = CallJS('Niels', str => {
            assert.equal(str, 'Hello Niels!');
            return 42;
        });
        assert.equal(ret, 42);
    }

    // Test with recursion
    {
        let recurse = (i, str, d) => {
            assert.equal(str, 'Hello!');
            assert.equal(d, 42.0);
            return i + (i ? CallRecursiveJS(i - 1, recurse) : 0);
        };
        let total = CallRecursiveJS(9, recurse);
        assert.equal(total, 45);
    }

    // And now, with a complex struct
    {
        let out = {};
        let bfg = ModifyBFG(2, 5, "Yo!", bfg => {
            bfg.inner.f *= -1;
            bfg.d = "New!";
            return bfg;
        }, out);
        assert.deepEqual(bfg, { a: 2, b: 4, c: -25, d: 'New!', e: 54, inner: { f: -10, g: 3 } });
        assert.deepEqual(out, { a: 2, b: 4, c: -25, d: 'X/Yo!/X', e: 54, inner: { f: 10, g: 3 } });
    }

    // With many parameters
    {
        let a = [], b = [], c = [], d = [], e = [], f = [], g = [], h = [];

        let fn = (i, v1, v2, v3, v4, v5, v6, v7, v8) => {
            a.push(v1);
            b.push(v2);
            c.push(v3);
            d.push(v4);
            e.push(v5);
            f.push(v6);
            g.push(v7);
            h.push(v8);

            if (i)
                Recurse8(i - 1, fn);
        };
        Recurse8(3, fn);

        assert.deepEqual(a, [3, 2, 1, 0]);
        assert.deepEqual(b, [6, 4, 2, 0]);
        assert.deepEqual(c, [4, 3, 2, 1]);
        assert.deepEqual(d, [7, 5, 3, 1]);
        assert.deepEqual(e, [0, 1, 2, 3]);
        assert.deepEqual(f, [103, 102, 101, 100]);
        assert.deepEqual(g, [1, 0, 1, 0]);
        assert.deepEqual(h, [-4, -3, -2, -1]);
    }

    // Stdcall callbacks
    {
        let ret = ApplyStd(1, 5, 9, (a, b, c) => a + b * c);
        assert.equal(ret, 46);
    }

    // Array of callbacks
    {
        let callbacks = [x => x * 5, x => x - 42, x => -x];
        let ret = ApplyMany(27, callbacks, callbacks.length);
        assert.equal(ret, -93);
    }

    // Struct of callbacks
    {
        let callbacks = { first: x => -x, second: x => x * 5, third: x => x - 42 };
        let ret = ApplyStruct(27, callbacks);
        assert.equal(ret, -177);
    }

    // Persistent callback
    {
        SetIndirect(x => -x);
        assert.throws(() => CallIndirect(27), { message: /non-registered callback/ });

        let cb = koffi.register(x => -x, koffi.pointer(IntCallback));
        SetIndirect(cb);
        assert.equal(CallIndirect(27), -27);

        assert.equal(koffi.unregister(cb), null);
        assert.throws(() => koffi.unregister(cb));
    }

    // Register with binding
    {
        class Multiplier {
            constructor(k) { this.k = k; }
            multiply(x) { return this.k * x; }
        }

        let mult = new Multiplier(5);
        let cb = koffi.register(mult, mult.multiply, 'IntCallback *');

        SetIndirect(cb);
        assert.equal(CallIndirect(42), 5 * 42);

        assert.equal(koffi.unregister(cb), null);
        assert.throws(() => koffi.unregister(cb));
    }

    // Test decoding callback pointers
    {
        assert.equal(koffi.offsetof('Vec2', 'x'), 0);
        assert.equal(koffi.offsetof('Vec2', 'y'), 8);

        let ret = MakeVectors(5, (len, ptr) => {
            assert.equal(len, 5);

            for (let i = 0; i < len; i++) {
                let x = koffi.decode(ptr, i * koffi.sizeof('Vec2'), 'double');
                let y = koffi.decode(ptr, i * koffi.sizeof('Vec2') + 8, 'double');

                assert.equal(x, i);
                assert.equal(y, -i);
            }

            for (let i = 0; i < len; i++) {
                let vec = koffi.decode(ptr, i * koffi.sizeof('Vec2'), 'Vec2');
                assert.deepEqual(vec, { x: i, y: -i });
            }

            let all = koffi.decode(ptr, koffi.array('double', 10, 'array'));
            assert.deepEqual(all, [0, 0, 1, -1, 2, -2, 3, -3, 4, -4]);

            return 424242;
        });

        assert.equal(ret, 424242);
    }

    // Test koffi.as() and koffi.decode() with qsort
    {
        let array = ['foo', 'bar', '123', 'foobar'];

        CallQSort(koffi.as(array, 'char **'), array.length, koffi.sizeof('void *'), koffi.as((ptr1, ptr2) => {
            let str1 = koffi.decode(ptr1, 'char *');
            let str2 = koffi.decode(ptr2, 'char *');

            return str1.localeCompare(str2);
        }, 'SortCallback *'));

        assert.deepEqual(array, ['123', 'bar', 'foo', 'foobar']);
    }

    // Make sure thread local CallData is restored after nested call inside callback
    // Regression test for issue #15
    {
        let chars = [97, 98];

        let cb = koffi.register((idx, c) => {
            assert.equal(GetMinusOne1(), -1);
            assert.equal(c, chars[idx]);

            return c;
        }, 'CharCallback *');

        let ret = CallMeChar(cb);
        assert.equal(ret, 97 + 98);

        // Don't unregister to make lingering trampolines don't make node crash on exit
        // koffi.unregister(cb);
    }

    // Use callbacks from secondary threads
    for (let i = 0; i < 1024; i++) {
        let cb = koffi.register(x => -x - 2, koffi.pointer(IntCallback));
        SetIndirect(cb);

        let results = await Promise.all([
            util.promisify(CallThreaded.async)(x => -x - 4, 27),
            util.promisify(CallThreaded.async)(null, 27)
        ]);

        assert.equal(results[0], -31);
        assert.equal(results[1], -29);

        koffi.unregister(cb);
    }

    // Encode callback output parameters
    {
        let src = [];
        for (let i = 0; i < 7; i++)
            src.push({ x: i, y: i * 2 });
        let vectors = [null, null, null, null, null, null, null];

        MakeVectorsIndirect(src.length, (len, out) => {
            koffi.encode(out, 'Vec2 [' + len + ']', src);
            return 0;
        }, vectors);

        assert.deepEqual(vectors, [
            { x: 0, y: 0 }, { x: 1, y: 2 }, { x: 2, y: 4 }, { x: 3, y: 6 },
            { x: 4, y: 8 }, { x: 5, y: 10 }, { x: 6, y: 12 }
        ]);
    }

    // Test callback encoding with a string
    {
        assert.equal(FmtRepeat((count, str) => {
            koffi.encode(count, 'int', 3);
            koffi.encode(str, 'const char *', 'Hello');
        }), 'HelloHelloHello');
    }

    // Run callback from event loop, on main thread
    {
        let called = false;
        let cb = koffi.register((idx, c) => { called = true; }, 'IdleCallback *');

        SetIdle(koffi.node.env, cb);
        TriggerIdle();

        assert.equal(called, false);
        await new Promise((resolve, reject) => setTimeout(resolve, 100));
        assert.equal(called, true);

        SetIdle(koffi.node.env, null);
    }
}
