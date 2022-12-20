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

const SimpleCallback = koffi.callback('int SimpleCallback(const char *str)');
const RecursiveCallback = koffi.callback('RecursiveCallback', 'float', ['int', 'str', 'double']);
const BigCallback = koffi.callback('BFG BigCallback(BFG bfg)');
const SuperCallback = koffi.callback('void SuperCallback(int i, int v1, double v2, int v3, int v4, int v5, int v6, float v7, int v8)');
const ApplyCallback = koffi.callback('int __stdcall ApplyCallback(int a, int b, int c)');
const IntCallback = koffi.callback('int IntCallback(int x)');
const VectorCallback = koffi.callback('int VectorCallback(int len, Vec2 *vec)');
const SortCallback = koffi.callback('int SortCallback(const void *ptr1, const void *ptr2)');

const StructCallbacks = koffi.struct('StructCallbacks', {
    first: koffi.pointer(IntCallback),
    second: 'IntCallback *',
    third: 'IntCallback *'
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
    const lib_filename = __dirname + '/build/misc' + koffi.extension;
    const lib = koffi.load(lib_filename);

    const CallJS = lib.func('int CallJS(const char *str, SimpleCallback *cb)');
    const CallRecursiveJS = lib.func('float CallRecursiveJS(int i, RecursiveCallback *func)');
    const ModifyBFG = lib.func('BFG ModifyBFG(int x, double y, const char *str, BigCallback *func, _Out_ BFG *p)');
    const Recurse8 = lib.func('void Recurse8(int i, SuperCallback *func)');
    const ApplyStd = lib.func('int ApplyStd(int a, int b, int c, ApplyCallback *func)');
    const ApplyMany = lib.func('int ApplyMany(int x, IntCallback **funcs, int length)');
    const ApplyStruct = lib.func('int ApplyStruct(int x, StructCallbacks callbacks)');
    const SetCallback = lib.func('void SetCallback(IntCallback *func)');
    const CallCallback = lib.func('int CallCallback(int x)');
    const MakeVectors = lib.func('int MakeVectors(int len, VectorCallback *func)');
    const CallQSort = lib.func('void CallQSort(_Inout_ void *base, size_t nmemb, size_t size, SortCallback *cb)');

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
        SetCallback(x => -x);
        assert.throws(() => CallCallback(27), { message: /non-registered callback/ });

        let cb = koffi.register(x => -x, koffi.pointer(IntCallback));
        SetCallback(cb);
        assert.equal(CallCallback(27), -27);

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

        SetCallback(cb);
        assert.equal(CallCallback(42), 5 * 42);

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

            let all = koffi.decode(ptr, koffi.array('double', 10, 'array'), 10);
            assert.deepEqual(all, [0, 0, 1, -1, 2, -2, 3, -3, 4, -4]);

            return 424242;
        });

        assert.equal(ret, 424242);
    }

    // Test koffi.as() and koffi.decode() with qsort
    {
        let array = ['foo', 'bar', '123', 'foobar'];

        CallQSort(koffi.as(array, 'char **'), array.length, koffi.sizeof('void *'), (ptr1, ptr2) => {
            let str1 = koffi.decode(ptr1, 'char *');
            let str2 = koffi.decode(ptr2, 'char *');

            return str1.localeCompare(str2);
        });

        assert.deepEqual(array, ['123', 'bar', 'foo', 'foobar']);
    }
}
