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
    let lib_filename = path.dirname(__filename) + '/../../build/misc' + koffi.extension;
    let lib = koffi.load(lib_filename);

    const FillPack3 = lib.cdecl('FillPack3', 'void', ['int', 'int', 'int', koffi.out(koffi.pointer(Pack3))]);
    const AddPack3 = lib.fastcall('AddPack3', 'void', ['int', 'int', 'int', koffi.inout(koffi.pointer(Pack3))]);

    let p = {};

    FillPack3(1, 2, 3, p);
    assert.deepEqual(p, { a: 1, b: 2, c: 3 });

    AddPack3(6, 9, -12, p);
    assert.deepEqual(p, { a: 7, b: 11, c: -9 });
}
