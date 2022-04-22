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

const koffi = require('../../build/koffi.node');
const assert = require('assert');
const path = require('path');

const Pack3 = koffi.struct('Pack3', {
    a: 'int',
    b: 'int',
    c: 'int'
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
    let lib_filename = path.dirname(__filename) + '/../build/misc' + koffi.extension;
    let lib = koffi.load(lib_filename);

    const FillPack3 = lib.cdecl('FillPack3', 'void', ['int', 'int', 'int', koffi.out(koffi.pointer(Pack3))]);
    const AddPack3 = lib.fastcall('AddPack3', 'void', ['int', 'int', 'int', koffi.inout(koffi.pointer(Pack3))]);
    const ConcatenateToInt1 = lib.cdecl('ConcatenateToInt1', 'int64_t', Array(12).fill('int8_t'));
    const ConcatenateToInt4 = lib.cdecl('ConcatenateToInt4', 'int64_t', Array(12).fill('int32_t'));
    const ConcatenateToInt8 = lib.cdecl('ConcatenateToInt8', 'int64_t', Array(12).fill('int64_t'));
    const ConcatenateToStr1 = lib.cdecl('ConcatenateToStr1', 'string', [...Array(8).fill('int8_t'), koffi.struct('IJK1', {i: 'int8_t', j: 'int8_t', k: 'int8_t'}), 'int8_t']);
    const ConcatenateToStr4 = lib.cdecl('ConcatenateToStr4', 'string', [...Array(8).fill('int32_t'), koffi.pointer(koffi.struct('IJK4', {i: 'int32_t', j: 'int32_t', k: 'int32_t'})), 'int32_t']);
    const ConcatenateToStr8 = lib.cdecl('ConcatenateToStr8', 'string', [...Array(8).fill('int64_t'), koffi.struct('IJK8', {i: 'int64_t', j: 'int64_t', k: 'int64_t'}), 'int64_t']);

    let p = {};

    FillPack3(1, 2, 3, p);
    assert.deepEqual(p, { a: 1, b: 2, c: 3 });

    AddPack3(6, 9, -12, p);
    assert.deepEqual(p, { a: 7, b: 11, c: -9 });

    assert.equal(ConcatenateToInt1(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);
    assert.equal(ConcatenateToInt4(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);
    assert.equal(ConcatenateToInt8(5, 6, 1, 2, 3, 9, 4, 4, 0, 6, 8, 7), 561239440687n);
    assert.equal(ConcatenateToStr1(5, 6, 1, 2, 3, 9, 4, 4, {i: 0, j: 6, k: 8}, 7), '561239440687');
    assert.equal(ConcatenateToStr4(5, 6, 1, 2, 3, 9, 4, 4, {i: 0, j: 6, k: 8}, 7), '561239440687');
    assert.equal(ConcatenateToStr8(5, 6, 1, 2, 3, 9, 4, 4, {i: 0, j: 6, k: 8}, 7), '561239440687');
}
