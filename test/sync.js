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

const Pack1 = koffi.struct('Pack1', {
    a: 'int'
});
const Pack2 = koffi.struct('Pack2', {
    a: 'int',
    b: 'int'
});
const Pack3 = koffi.struct('Pack3', {
    a: 'int',
    b: 'int',
    c: 'int'
});

const Float2 = koffi.struct('Float2', {
    a: 'float',
    b: 'float'
});
const Float3 = koffi.struct('Float3', {
    a: 'float',
    b: koffi.array('float', 2)
});

const Double2 = koffi.struct('Double2', {
    a: 'double',
    b: 'double'
});
const Double3 = koffi.struct('Double3', {
    a: 'double',
    s: koffi.struct({
        b: 'double',
        c: 'double'
    })
});

const FloatInt = koffi.struct('FloatInt', {
    f: 'float',
    i: 'int'
});
const IntFloat = koffi.struct('IntFloat', {
    i: 'int',
    f: 'float'
});

const BFG = koffi.struct('BFG', {
    a: 'int8_t',
    b: 'int64_t',
    c: 'char',
    d: 'string',
    e: 'short',
    inner: koffi.struct({
        f: 'float',
        g: 'double'
    })
});
const PackedBFG = koffi.pack('PackedBFG', {
    a: 'int8_t',
    b: 'int64_t',
    c: 'char',
    d: 'string',
    e: 'short',
    inner: koffi.pack({
        f: 'float',
        g: 'double'
    })
});

const FixedString = koffi.struct('FixedString', {
    buf: koffi.array('int8', 64)
});
const FixedString2 = koffi.struct('FixedString2', {
    buf: koffi.array('char', 64)
});

const FixedWide = koffi.struct('FixedWide', {
    buf: koffi.array('char16', 64, 'typed')
});
const FixedWide2 = koffi.struct('FixedWide2', {
    buf: koffi.array('char16', 64)
});

const SingleU32 = koffi.struct('SingleU32', { v: 'uint32_t' });
const SingleU64 = koffi.struct('SingleU64', { v: 'uint64_t' });
const SingleI64 = koffi.struct('SingleI64', { v: 'int64_t' });

const IntContainer = koffi.struct('IntContainer', {
    values: koffi.array('int', 16),
    len: 'int'
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
    let lib_filename = path.dirname(__filename) + '/build/misc' + koffi.extension;
    let lib = koffi.load(lib_filename);

    const FillPack1 = lib.func('FillPack1', 'void', ['int', koffi.out(koffi.pointer(Pack1))]);
    const RetPack1 = lib.func('RetPack1', Pack1, ['int']);
    const AddPack1 = lib.fastcall('AddPack1', 'void', ['int', koffi.inout(koffi.pointer(Pack1))]);
    const FillPack2 = lib.func('FillPack2', 'void', ['int', 'int', koffi.out(koffi.pointer(Pack2))]);
    const RetPack2 = lib.func('RetPack2', Pack2, ['int', 'int']);
    const AddPack2 = lib.fastcall('AddPack2', 'void', ['int', 'int', koffi.inout(koffi.pointer(Pack2))]);
    const FillPack3 = lib.func('FillPack3', 'void', ['int', 'int', 'int', koffi.out(koffi.pointer(Pack3))]);
    const RetPack3 = lib.func('RetPack3', Pack3, ['int', 'int', 'int']);
    const AddPack3 = lib.fastcall('AddPack3', 'void', ['int', 'int', 'int', koffi.inout(koffi.pointer(Pack3))]);
    const PackFloat2 = lib.func('Float2 PackFloat2(float a, float b, _Out_ Float2 *out)');
    const ThroughFloat2 = lib.func('Float2 ThroughFloat2(Float2 f2)');
    const PackFloat3 = lib.func('Float3 PackFloat3(float a, float b, float c, _Out_ Float3 *out)');
    const ThroughFloat3 = lib.func('Float3 ThroughFloat3(Float3 f3)');
    const PackDouble2 = lib.func('Double2 PackDouble2(double a, double b, _Out_ Double2 *out)');
    const PackDouble3 = lib.func('Double3 PackDouble3(double a, double b, double c, _Out_ Double3 *out)');
    const ReverseFloatInt = lib.func('IntFloat ReverseFloatInt(FloatInt sfi)');
    const ReverseIntFloat = lib.func('FloatInt ReverseIntFloat(IntFloat sif)');
    const ConcatenateToInt1 = lib.func('ConcatenateToInt1', 'int64_t', Array(12).fill('int8_t'));
    const ConcatenateToInt4 = lib.func('ConcatenateToInt4', 'int64_t', Array(12).fill('int32_t'));
    const ConcatenateToInt8 = lib.func('ConcatenateToInt8', 'int64_t', Array(12).fill('int64_t'));
    const ConcatenateToStr1 = lib.func('ConcatenateToStr1', 'string', [...Array(8).fill('int8_t'), koffi.struct('IJK1', {i: 'int8_t', j: 'int8_t', k: 'int8_t'}), 'int8_t']);
    const ConcatenateToStr4 = lib.func('ConcatenateToStr4', 'string', [...Array(8).fill('int32_t'), koffi.pointer(koffi.struct('IJK4', {i: 'int32_t', j: 'int32_t', k: 'int32_t'})), 'int32_t']);
    const ConcatenateToStr8 = lib.func('ConcatenateToStr8', 'string', [...Array(8).fill('int64_t'), koffi.struct('IJK8', {i: 'int64_t', j: 'int64_t', k: 'int64_t'}), 'int64_t']);
    const MakeBFG = lib.func('BFG __stdcall MakeBFG(_Out_ BFG *p, int x, double y, const char *str)');
    const MakePackedBFG = lib.func('PackedBFG __fastcall MakePackedBFG(int x, double y, _Out_ PackedBFG *p, const char *str)');
    const ReturnBigString = process.platform == 'win32' ?
                            lib.stdcall(1, 'string', ['string']) :
                            lib.func('const char * __stdcall ReturnBigString(const char *str)');
    const PrintFmt = lib.func('const char *PrintFmt(const char *fmt, ...)');
    const Concat16 = lib.func('const char16_t *Concat16(const char16_t *str1, const char16_t *str2)')
    const ReturnFixedStr = lib.func('FixedString ReturnFixedStr(FixedString str)');
    const ReturnFixedStr2 = lib.func('FixedString2 ReturnFixedStr(FixedString2 str)');
    const ReturnFixedWide = lib.func('FixedWide ReturnFixedWide(FixedWide str)');
    const ReturnFixedWide2 = lib.func('FixedWide2 ReturnFixedWide(FixedWide2 str)');
    const ThroughUInt32UU = lib.func('uint32_t ThroughUInt32UU(uint32_t v)');
    const ThroughUInt32SS = lib.func('SingleU32 ThroughUInt32SS(SingleU32 s)');
    const ThroughUInt32SU = lib.func('SingleU32 ThroughUInt32SU(uint32_t v)');
    const ThroughUInt32US = lib.func('uint32_t ThroughUInt32US(SingleU32 s)');
    const ThroughUInt64UU = lib.func('uint64_t ThroughUInt64UU(uint64_t v)');
    const ThroughUInt64SS = lib.func('SingleU64 ThroughUInt64SS(SingleU64 s)');
    const ThroughUInt64SU = lib.func('SingleU64 ThroughUInt64SU(uint64_t v)');
    const ThroughUInt64US = lib.func('uint64_t ThroughUInt64US(SingleU64 s)');
    const ThroughInt64II = lib.func('int64_t ThroughInt64II(int64_t v)');
    const ThroughInt64SS = lib.func('SingleI64 ThroughInt64SS(SingleI64 s)');
    const ThroughInt64SI = lib.func('SingleI64 ThroughInt64SI(int64_t v)');
    const ThroughInt64IS = lib.func('int64_t ThroughInt64IS(SingleI64 s)');
    const ArrayToStruct = lib.func('IntContainer ArrayToStruct(int *ptr, int len)');
    const FillRange = lib.func('void FillRange(int init, int step, _Out_ int *out, int len)');
    const MultiplyIntegers = lib.func('void MultiplyIntegers(int multiplier, _Inout_ int *values, int len)');

    // Simple tests with Pack1
    {
        let p = {};

        FillPack1(777, p);
        assert.deepEqual(p, { a: 777 });

        let q = RetPack1(6);
        assert.deepEqual(q, { a: 6 });

        AddPack1(6, p);
        assert.deepEqual(p, { a: 783 });
    }

    // Simple tests with Pack2
    {
        let p = {};

        FillPack2(123, 456, p);
        assert.deepEqual(p, { a: 123, b: 456 });

        let q = RetPack2(6, 9);
        assert.deepEqual(q, { a: 6, b: 9 });

        AddPack2(6, 9, p);
        assert.deepEqual(p, { a: 129, b: 465 });
    }

    // Simple tests with Pack3
    {
        let p = {};

        FillPack3(1, 2, 3, p);
        assert.deepEqual(p, { a: 1, b: 2, c: 3 });

        let q = RetPack3(6, 9, -12);
        assert.deepEqual(q, { a: 6, b: 9, c: -12 });

        AddPack3(6, 9, -12, p);
        assert.deepEqual(p, { a: 7, b: 11, c: -9 });
    }

    // HFA tests
    {
        let f2p = {};
        let f2 = PackFloat2(1.5, 3.0, f2p);
        assert.deepEqual(f2, { a: 1.5, b: 3.0 });
        assert.deepEqual(f2, f2p);
        assert.deepEqual(ThroughFloat2({ a: 1.5, b: 3.0 }), f2);
        assert.deepEqual(ThroughFloat2(f2), f2);

        let f3p = {};
        let f3 = PackFloat3(20.0, 30.0, 40.0, f3p);
        assert.deepEqual(f3, { a: 20.0, b: Float32Array.from([30.0, 40.0]) });
        assert.deepEqual(f3, f3p);
        assert.deepEqual(ThroughFloat3({ a: 20.0, b: [30.0, 40.0] }), f3);
        assert.deepEqual(ThroughFloat3(f3), f3);

        let d2p = {};
        let d2 = PackDouble2(1.0, 2.0, d2p);
        assert.deepEqual(d2, { a: 1.0, b: 2.0 });
        assert.deepEqual(d2, d2p);

        let d3p = {};
        let d3 = PackDouble3(0.5, 10.0, 5.0, d3p);
        assert.deepEqual(d3, { a: 0.5, s: { b: 10.0, c: 5.0 } });
        assert.deepEqual(d3, d3p);
    }

    // Mixed int/float structs
    {
        let sif = { i: 4, f: 2.0 };
        assert.deepEqual(ReverseIntFloat(sif), { i: 2, f: 4 });
        assert.deepEqual(ReverseFloatInt(sif), { i: 2, f: 4 });
    }

    // Many parameters
    {
        assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687);
        assert.equal(ConcatenateToInt4(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687);
        assert.equal(ConcatenateToInt8(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687);
        assert.equal(ConcatenateToStr1(5, 6, 1, 2, 3, 9, 4, 4, {i: 0, j: 6, k: 8}, 7), '561239440687');
        assert.equal(ConcatenateToStr4(5, 6, 1, 2, 3, 9, 4, 4, {i: 0, j: 6, k: 8}, 7), '561239440687');
        assert.equal(ConcatenateToStr8(5, 6, 1, 2, 3, 9, 4, 4, {i: 0, j: 6, k: 8}, 7), '561239440687');
    }

    // Big struct
    {
        let out = {};
        let bfg = MakeBFG(out, 2, 7, '__Hello123456789++++foobarFOOBAR!__');
        assert.deepEqual(bfg, { a: 2, b: 4, c: -25, d: 'X/__Hello123456789++++foobarFOOBAR!__/X', e: 54, inner: { f: 14, g: 5 } });
        assert.deepEqual(out, bfg);
    }

    // Packed struct
    {
        let out = {};
        let bfg = MakePackedBFG(2, 7, out, '__Hello123456789++++foobarFOOBAR!__');
        assert.deepEqual(bfg, { a: 2, b: 4, c: -25, d: 'X/__Hello123456789++++foobarFOOBAR!__/X', e: 54, inner: { f: 14, g: 5 } });
        assert.deepEqual(out, bfg);
    }

    // Big string
    {
        let str = 'fooBAR!'.repeat(1024 * 1024);
        assert.equal(ReturnBigString(str), str);
    }

    // Variadic
    {
        let str = PrintFmt('foo %d %g %s', 'int', 200, 'double', 1.5, 'string', 'BAR');
        assert.equal(str, 'foo 200 1.5 BAR');
    }

    // UTF-16LE strings
    {
        let str = Concat16('Hello ', 'World!');
        assert.equal(str, 'Hello World!');
    }

    // String to/from fixed-size buffers
    {
        let str = { buf: 'Hello!' };
        assert.deepEqual(ReturnFixedStr(str), { buf: Int8Array.from([72, 101, 108, 108, 111, 33, ...Array(58).fill(0)]) });
        assert.deepEqual(ReturnFixedStr2(str), { buf: 'Hello!' });
        assert.deepEqual(ReturnFixedWide(str), { buf: Int16Array.from([72, 101, 108, 108, 111, 33, ...Array(58).fill(0)]) });
        assert.deepEqual(ReturnFixedWide2(str), { buf: 'Hello!' });
    }

    // Big numbers
    {
        assert.equal(ThroughUInt32UU(4294967284), 4294967284);
        assert.deepEqual(ThroughUInt32SS({ v: 4294967284 }), { v: 4294967284 });
        assert.deepEqual(ThroughUInt32SU(4294967284), { v: 4294967284 });
        assert.equal(ThroughUInt32US({ v: 4294967284 }), 4294967284);

        assert.equal(ThroughUInt64UU(9007199254740989), 9007199254740989);
        assert.deepEqual(ThroughUInt64SS({ v: 9007199254740989 }), { v: 9007199254740989 });
        assert.deepEqual(ThroughUInt64SU(9007199254740989), { v: 9007199254740989 });
        assert.equal(ThroughUInt64US({ v: 9007199254740989 }), 9007199254740989);

        assert.equal(ThroughUInt64UU(18446744073709551598n), 18446744073709551598n);
        assert.deepEqual(ThroughUInt64SS({ v: 18446744073709551598n }), { v: 18446744073709551598n });
        assert.deepEqual(ThroughUInt64SU(18446744073709551598n), { v: 18446744073709551598n });
        assert.equal(ThroughUInt64US({ v: 18446744073709551598n }), 18446744073709551598n);

        assert.equal(ThroughInt64II(-9007199254740989), -9007199254740989);
        assert.deepEqual(ThroughInt64SS({ v: -9007199254740989 }), { v: -9007199254740989 });
        assert.deepEqual(ThroughInt64SI(-9007199254740989), { v: -9007199254740989 });
        assert.equal(ThroughInt64IS({ v: -9007199254740989 }), -9007199254740989);

        assert.equal(ThroughInt64II(-9223372036854775803n), -9223372036854775803n);
        assert.deepEqual(ThroughInt64SS({ v: -9223372036854775803n }), { v: -9223372036854775803n });
        assert.deepEqual(ThroughInt64SI(-9223372036854775803n), { v: -9223372036854775803n });
        assert.equal(ThroughInt64IS({ v: -9223372036854775803n }), -9223372036854775803n);
    }

    // Array pointers as input
    {
        let arr1 = [5, 7, 8, 4];
        let arr2 = Int32Array.from([8, 454, 6, 3, 45]);

        assert.deepEqual(ArrayToStruct(arr1, 0), { values: Int32Array.from(Array(16).fill(0)), len: 0 });
        assert.deepEqual(ArrayToStruct(arr1, 3), { values: Int32Array.from([5, 7, 8, ...Array(13).fill(0)]), len: 3 });
        assert.deepEqual(ArrayToStruct(arr2, 4), { values: Int32Array.from([8, 454, 6, 3, ...Array(12).fill(0)]), len: 4 });
        assert.deepEqual(ArrayToStruct(arr2, 2), { values: Int32Array.from([8, 454, ...Array(14).fill(0)]), len: 2 });
    }

    // Array pointers as output
    {
        let out1 = Array(10);
        let out2 = new Int32Array(10);
        let mult = -3;

        FillRange(2, 7, out1, out1.length);
        FillRange(13, 3, out2, out2.length);

        assert.deepEqual(out1, [2, 9, 16, 23, 30, 37, 44, 51, 58, 65]);
        assert.deepEqual(out2, new Int32Array([13, 16, 19, 22, 25, 28, 31, 34, 37, 40]));

        MultiplyIntegers(-1, out1, out1.length - 2);
        MultiplyIntegers(3, out2, out2.length - 3);
        assert.deepEqual(out1, [-2, -9, -16, -23, -30, -37, -44, -51, 58, 65]);
        assert.deepEqual(out2, new Int32Array([3 * 13, 3 * 16, 3 * 19, 3 * 22, 3 * 25, 3 * 28, 3 * 31, 34, 37, 40]));
    }
}
