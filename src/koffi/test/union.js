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

// Make sure Koffi only uses object own properties, and ignores prototype properties
Object.defineProperty(globalThis.Object.prototype, 'stuff', {
    value: function() {},
    writable: false,
    configurable: false,
    enumerable: true
});

const SingleU = koffi.union('SingleU', {
    f: 'float'
});

const DualU = koffi.union('DualU', {
    d: 'double',
    u: 'uint64_t'
});

const MultiU = koffi.union('MultiU', {
    d: 'double',
    f2: koffi.array('float', 2),
    u: 'uint64_t',
    raw: koffi.array('uint8_t', 8),
    st: koffi.struct({
        a: 'short',
        b: 'char',
        c: 'char',
        d: 'int'
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
    let lib_filename = path.join(__dirname, cnoke.output, 'union' + koffi.extension);
    let lib = koffi.load(lib_filename);

    const MakeSingleU = lib.func('SingleU MakeSingleU(float f)');
    const MakeSingleUIndirect = lib.func('MakeSingleUIndirect', 'void', ['float', '_Out_ SingleU *']);
    const MakeDualU = lib.func('DualU MakeDualU(double d)');
    const MakeDualUIndirect = lib.func('void MakeDualUIndirect(double d, _Out_ DualU *out)');
    const MakeMultiU = lib.func('MultiU MakeMultiU(float a, float b)');
    const MakeMultiUIndirect = lib.func('void MakeMultiUIndirect(float a, float b, _Out_ MultiU *out)');
    const GetMultiDouble = lib.func('float GetMultiDouble(MultiU u)');
    const GetMultiUnsigned = lib.func('float GetMultiUnsigned(MultiU u)');

    // Make direct single union
    {
        let u = MakeSingleU(-2);

        assert.equal(u.f, -2);
    }

    // Make indirect single union
    {
        let ptr = [null];
        MakeSingleUIndirect(42, ptr);
        let u = ptr[0];

        assert.equal(u.f, 42);
    }

    // Make direct dual union
    {
        let u = MakeDualU(-2);

        assert.equal(u.u, 13835058055282163712n);
        assert.equal(u.d, -2);
    }

    // Make indirect dual union
    {
        let ptr = [null];
        MakeDualUIndirect(42, ptr);
        let u = ptr[0];

        assert.equal(u.u, 4631107791820423168n);
        assert.equal(u.d, 42);
    }

    // Make direct multi union
    {
        let u = MakeMultiU(12, 5);

        assert.equal(u.u, 4656722015795806208n);
        assert.deepEqual(u.f2, Float32Array.from([12, 5]));
        assert.deepEqual(u.raw, Uint8Array.from([0, 0, 64, 65, 0, 0, 160, 64]));
        assert.deepEqual(u.st, { a: 0, b: 64, c: 65, d: 1084227584 });
    }

    // Make indirect multi union
    {
        let ptr = [null];
        MakeMultiUIndirect(1, 3, ptr);
        let u = ptr[0];

        assert.equal(u.u, 4629700418002223104n);
        assert.deepEqual(u.f2, Float32Array.from([1, 3]));
        assert.deepEqual(u.raw, Uint8Array.from([0, 0, 128, 63, 0, 0, 64, 64]));
        assert.deepEqual(u.st, { a: 0, b: -128, c: 63, d: 1077936128 });
    }

    // Test union passing with objects
    assert.equal(GetMultiUnsigned({ f2: [5, -4] }), 0xC080000000000140);
    assert.equal(GetMultiDouble({ d: 450 }), 450);
    assert.equal(GetMultiUnsigned({ d: 450 }), 0x407C1FFFFFFFFFB0);
    assert.equal(GetMultiDouble({ u: 18 }), 0);
    assert.equal(GetMultiUnsigned({ u: 18 }), 18);

    // Test union passing with new Union
    {
        assert.equal(GetMultiUnsigned(make_union('f2', [5, -4])), 0xC080000000000140);
        assert.equal(GetMultiDouble(make_union('d', 450)), 450);
        assert.equal(GetMultiUnsigned(make_union('d', 450)), 0x407C1FFFFFFFFFB0);
        assert.equal(GetMultiDouble(make_union('u', 18)), 0);
        assert.equal(GetMultiUnsigned(make_union('u', 18)), 18);

        function make_union(member, value) {
            let u = new koffi.Union(MultiU);
            u[member] = value;
            return u;
        }
    }
}
